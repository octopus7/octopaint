#include "pch.h"
#include "D3DCanvasRenderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include <d2d1_3.h>
#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <microsoft.ui.xaml.media.dxinterop.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace octopaint::winui
{
    namespace
    {
        constexpr DXGI_FORMAT CanvasFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
        constexpr float CheckerSize = 12.0F;
        constexpr float SelectionDashLength = 4.0F;
        constexpr float SelectionDashPeriod = SelectionDashLength * 2.0F;

        [[nodiscard]] std::uint32_t PixelSize(float logical_size, float rasterization_scale)
        {
            if (!std::isfinite(logical_size) || !std::isfinite(rasterization_scale) ||
                logical_size < 0.0F || rasterization_scale <= 0.0F)
            {
                throw std::invalid_argument("Canvas dimensions and rasterization scale must be finite and non-negative.");
            }

            if (logical_size == 0.0F)
            {
                return 0;
            }

            auto const pixels = std::ceil(static_cast<double>(logical_size) * rasterization_scale);
            if (pixels > static_cast<double>((std::numeric_limits<std::uint32_t>::max)()))
            {
                throw std::overflow_error("Canvas pixel size is too large.");
            }
            return static_cast<std::uint32_t>(pixels);
        }

        [[nodiscard]] bool IsDeviceLost(HRESULT const result) noexcept
        {
            return result == DXGI_ERROR_DEVICE_REMOVED ||
                result == DXGI_ERROR_DEVICE_RESET ||
                result == DXGI_ERROR_DRIVER_INTERNAL_ERROR;
        }
    }

    class D3DCanvasRenderer::Impl final
    {
    public:
        void Attach(winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel const& panel)
        {
            if (!panel)
            {
                throw std::invalid_argument("A valid SwapChainPanel is required.");
            }

            panel_ = panel;
            CreateDeviceResources();
            CreateOrResizeSwapChain();
        }

        void Resize(float const logical_width, float const logical_height, float const rasterization_scale)
        {
            auto const pixel_width = PixelSize(logical_width, rasterization_scale);
            auto const pixel_height = PixelSize(logical_height, rasterization_scale);

            logical_width_ = logical_width;
            logical_height_ = logical_height;
            rasterization_scale_ = rasterization_scale;

            if (pixel_width == pixel_width_ && pixel_height == pixel_height_)
            {
                UpdateDocumentRect();
                return;
            }

            pixel_width_ = pixel_width;
            pixel_height_ = pixel_height;
            CreateOrResizeSwapChain();
            UpdateDocumentRect();
        }

        void SetDocument(DocumentBitmapView const& document)
        {
            ValidateDocument(document);

            if (document_width_ != document.width || document_height_ != document.height)
            {
                selection_segments_.clear();
            }
            document_width_ = document.width;
            document_height_ = document.height;
            document_stride_ = document.stride;
            document_pixels_.assign(document.premultiplied_bgra.begin(), document.premultiplied_bgra.end());
            CreateDocumentBitmap();
            UpdateDocumentRect();
        }

        void ClearDocument() noexcept
        {
            document_width_ = 0;
            document_height_ = 0;
            document_stride_ = 0;
            document_pixels_.clear();
            document_bitmap_ = nullptr;
            document_rect_ = {};
            selection_segments_.clear();
        }

        void SetSelectionOutline(std::span<SelectionEdgeSegment const> const segments)
        {
            std::vector<SelectionEdgeSegment> validated;
            validated.reserve(segments.size());
            for (auto const& segment : segments)
            {
                if (!std::isfinite(segment.x1) || !std::isfinite(segment.y1) ||
                    !std::isfinite(segment.x2) || !std::isfinite(segment.y2))
                {
                    throw std::invalid_argument("Selection edge coordinates must be finite.");
                }

                auto const horizontal = segment.y1 == segment.y2;
                auto const vertical = segment.x1 == segment.x2;
                if (!horizontal && !vertical)
                {
                    throw std::invalid_argument("Selection edges must be horizontal or vertical.");
                }
                if (segment.x1 == segment.x2 && segment.y1 == segment.y2)
                {
                    continue;
                }
                validated.push_back(segment);
            }
            selection_segments_ = std::move(validated);
        }

        void ClearSelectionOutline() noexcept
        {
            selection_segments_.clear();
        }

        void SetSelectionAnimationPhase(float const phase_pixels) noexcept
        {
            if (!std::isfinite(phase_pixels))
            {
                return;
            }
            selection_phase_ = std::fmod(phase_pixels, SelectionDashPeriod);
            if (selection_phase_ < 0.0F)
            {
                selection_phase_ += SelectionDashPeriod;
            }
        }

        void AdvanceSelectionAnimationPhase(float const delta_pixels) noexcept
        {
            if (std::isfinite(delta_pixels))
            {
                SetSelectionAnimationPhase(selection_phase_ + delta_pixels);
            }
        }

        [[nodiscard]] float SelectionAnimationPhase() const noexcept
        {
            return selection_phase_;
        }

        [[nodiscard]] bool Render()
        {
            if (!panel_ || pixel_width_ == 0 || pixel_height_ == 0)
            {
                return false;
            }

            if (!swap_chain_ || !d2d_context_ || !target_bitmap_)
            {
                CreateDeviceResources();
                CreateOrResizeSwapChain();
            }

            if (!target_bitmap_)
            {
                return false;
            }

            d2d_context_->BeginDraw();
            d2d_context_->SetTransform(D2D1::Matrix3x2F::Identity());
            d2d_context_->Clear(D2D1::ColorF(0x202124));

            if (document_width_ != 0 && document_height_ != 0)
            {
                DrawDocument();
                DrawSelectionOutline();
            }

            auto result = d2d_context_->EndDraw();
            if (result == D2DERR_RECREATE_TARGET || IsDeviceLost(result))
            {
                RecoverDevice();
                return false;
            }
            winrt::check_hresult(result);

            result = swap_chain_->Present(1, 0);
            if (IsDeviceLost(result))
            {
                RecoverDevice();
                return false;
            }
            winrt::check_hresult(result);
            return true;
        }

        [[nodiscard]] std::optional<DocumentPixelPoint> TryMapPanelToDocument(
            float const panel_x,
            float const panel_y) const noexcept
        {
            if (document_width_ == 0 || document_height_ == 0 ||
                document_rect_.width <= 0.0F || document_rect_.height <= 0.0F ||
                !std::isfinite(panel_x) || !std::isfinite(panel_y) ||
                panel_x < document_rect_.x || panel_y < document_rect_.y ||
                panel_x >= document_rect_.x + document_rect_.width ||
                panel_y >= document_rect_.y + document_rect_.height)
            {
                return std::nullopt;
            }

            auto const normalized_x = (panel_x - document_rect_.x) / document_rect_.width;
            auto const normalized_y = (panel_y - document_rect_.y) / document_rect_.height;
            auto const x = (std::min)(
                static_cast<std::uint32_t>(normalized_x * document_width_),
                document_width_ - 1);
            auto const y = (std::min)(
                static_cast<std::uint32_t>(normalized_y * document_height_),
                document_height_ - 1);
            return DocumentPixelPoint{ x, y };
        }

        [[nodiscard]] CanvasDocumentRect DocumentRectInPanel() const noexcept
        {
            return document_rect_;
        }

    private:
        void ValidateDocument(DocumentBitmapView const& document) const
        {
            if (document.width == 0 || document.height == 0)
            {
                throw std::invalid_argument("A document bitmap must have non-zero dimensions.");
            }

            auto const minimum_stride = static_cast<std::uint64_t>(document.width) * 4ULL;
            auto const required_bytes = static_cast<std::uint64_t>(document.stride) * document.height;
            if (document.stride < minimum_stride ||
                required_bytes > document.premultiplied_bgra.size())
            {
                throw std::invalid_argument("The premultiplied BGRA8 document buffer is too small.");
            }
        }

        void CreateDeviceResources()
        {
            ReleaseDeviceResources();

            constexpr std::array feature_levels{
                D3D_FEATURE_LEVEL_11_1,
                D3D_FEATURE_LEVEL_11_0,
                D3D_FEATURE_LEVEL_10_1,
                D3D_FEATURE_LEVEL_10_0,
            };

            D3D_FEATURE_LEVEL selected_level{};
            auto result = D3D11CreateDevice(
                nullptr,
                D3D_DRIVER_TYPE_HARDWARE,
                nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                feature_levels.data(),
                static_cast<UINT>(feature_levels.size()),
                D3D11_SDK_VERSION,
                d3d_device_.put(),
                &selected_level,
                d3d_context_.put());

            if (FAILED(result))
            {
                winrt::check_hresult(D3D11CreateDevice(
                    nullptr,
                    D3D_DRIVER_TYPE_WARP,
                    nullptr,
                    D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                    feature_levels.data(),
                    static_cast<UINT>(feature_levels.size()),
                    D3D11_SDK_VERSION,
                    d3d_device_.put(),
                    &selected_level,
                    d3d_context_.put()));
            }

            winrt::check_hresult(D2D1CreateFactory(
                D2D1_FACTORY_TYPE_SINGLE_THREADED,
                d2d_factory_.put()));

            auto const dxgi_device = d3d_device_.as<IDXGIDevice>();
            winrt::check_hresult(d2d_factory_->CreateDevice(dxgi_device.get(), d2d_device_.put()));
            winrt::check_hresult(d2d_device_->CreateDeviceContext(
                D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
                d2d_context_.put()));
            d2d_context_->SetDpi(96.0F, 96.0F);

            winrt::check_hresult(d2d_context_->CreateSolidColorBrush(
                D2D1::ColorF(0xFFFFFF),
                checker_light_.put()));
            winrt::check_hresult(d2d_context_->CreateSolidColorBrush(
                D2D1::ColorF(0xD8D8D8),
                checker_dark_.put()));
            winrt::check_hresult(d2d_context_->CreateSolidColorBrush(
                D2D1::ColorF(0x000000),
                selection_black_.put()));
            winrt::check_hresult(d2d_context_->CreateSolidColorBrush(
                D2D1::ColorF(0xFFFFFF),
                selection_white_.put()));
        }

        void ReleaseDeviceResources() noexcept
        {
            if (d2d_context_)
            {
                d2d_context_->SetTarget(nullptr);
            }
            target_bitmap_ = nullptr;
            document_bitmap_ = nullptr;
            checker_light_ = nullptr;
            checker_dark_ = nullptr;
            selection_black_ = nullptr;
            selection_white_ = nullptr;
            swap_chain_ = nullptr;
            d2d_context_ = nullptr;
            d2d_device_ = nullptr;
            d2d_factory_ = nullptr;
            d3d_context_ = nullptr;
            d3d_device_ = nullptr;
        }

        void CreateOrResizeSwapChain()
        {
            target_bitmap_ = nullptr;
            if (d2d_context_)
            {
                d2d_context_->SetTarget(nullptr);
            }

            if (!panel_ || pixel_width_ == 0 || pixel_height_ == 0)
            {
                return;
            }

            if (!d3d_device_)
            {
                CreateDeviceResources();
            }

            if (swap_chain_)
            {
                d3d_context_->ClearState();
                d3d_context_->Flush();
                auto const result = swap_chain_->ResizeBuffers(
                    2,
                    pixel_width_,
                    pixel_height_,
                    CanvasFormat,
                    0);
                if (IsDeviceLost(result))
                {
                    RecoverDevice();
                    return;
                }
                winrt::check_hresult(result);
            }
            else
            {
                auto const dxgi_device = d3d_device_.as<IDXGIDevice>();
                winrt::com_ptr<IDXGIAdapter> adapter;
                winrt::check_hresult(dxgi_device->GetAdapter(adapter.put()));
                winrt::com_ptr<IDXGIFactory2> factory;
                winrt::check_hresult(adapter->GetParent(IID_PPV_ARGS(factory.put())));

                DXGI_SWAP_CHAIN_DESC1 description{};
                description.Width = pixel_width_;
                description.Height = pixel_height_;
                description.Format = CanvasFormat;
                description.SampleDesc.Count = 1;
                description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                description.BufferCount = 2;
                description.Scaling = DXGI_SCALING_STRETCH;
                description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
                description.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

                winrt::com_ptr<IDXGISwapChain1> swap_chain;
                winrt::check_hresult(factory->CreateSwapChainForComposition(
                    d3d_device_.get(),
                    &description,
                    nullptr,
                    swap_chain.put()));
                swap_chain_ = swap_chain;

                if (auto const panel_native2 = panel_.try_as<ISwapChainPanelNative2>())
                {
                    winrt::check_hresult(panel_native2->SetSwapChain(swap_chain_.get()));
                }
                else if (auto const panel_native = panel_.try_as<ISwapChainPanelNative>())
                {
                    winrt::check_hresult(panel_native->SetSwapChain(swap_chain_.get()));
                }
                else
                {
                    swap_chain_ = nullptr;
                    return;
                }
            }

            CreateTargetBitmap();
            CreateDocumentBitmap();
        }

        void CreateTargetBitmap()
        {
            winrt::com_ptr<IDXGISurface> surface;
            winrt::check_hresult(swap_chain_->GetBuffer(0, IID_PPV_ARGS(surface.put())));

            D2D1_BITMAP_PROPERTIES1 properties{};
            properties.pixelFormat = D2D1::PixelFormat(CanvasFormat, D2D1_ALPHA_MODE_IGNORE);
            properties.dpiX = 96.0F;
            properties.dpiY = 96.0F;
            properties.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
            winrt::check_hresult(d2d_context_->CreateBitmapFromDxgiSurface(
                surface.get(),
                &properties,
                target_bitmap_.put()));
            d2d_context_->SetTarget(target_bitmap_.get());
        }

        void CreateDocumentBitmap()
        {
            document_bitmap_ = nullptr;
            if (!d2d_context_ || document_pixels_.empty())
            {
                return;
            }

            D2D1_BITMAP_PROPERTIES1 properties{};
            properties.pixelFormat = D2D1::PixelFormat(CanvasFormat, D2D1_ALPHA_MODE_PREMULTIPLIED);
            properties.dpiX = 96.0F;
            properties.dpiY = 96.0F;
            properties.bitmapOptions = D2D1_BITMAP_OPTIONS_NONE;
            winrt::check_hresult(d2d_context_->CreateBitmap(
                D2D1::SizeU(document_width_, document_height_),
                document_pixels_.data(),
                document_stride_,
                &properties,
                document_bitmap_.put()));
        }

        void DrawDocument()
        {
            auto const scale = rasterization_scale_;
            D2D1_RECT_F const destination{
                document_rect_.x * scale,
                document_rect_.y * scale,
                (document_rect_.x + document_rect_.width) * scale,
                (document_rect_.y + document_rect_.height) * scale,
            };

            d2d_context_->FillRectangle(destination, checker_light_.get());
            auto const checker_pixels = CheckerSize * scale;
            for (float y = destination.top; y < destination.bottom; y += checker_pixels)
            {
                auto const row = static_cast<std::uint32_t>((y - destination.top) / checker_pixels);
                for (float x = destination.left; x < destination.right; x += checker_pixels)
                {
                    auto const column = static_cast<std::uint32_t>((x - destination.left) / checker_pixels);
                    if (((row + column) & 1U) == 0U)
                    {
                        continue;
                    }

                    d2d_context_->FillRectangle(
                        D2D1::RectF(
                            x,
                            y,
                            (std::min)(x + checker_pixels, destination.right),
                            (std::min)(y + checker_pixels, destination.bottom)),
                        checker_dark_.get());
                }
            }

            if (document_bitmap_)
            {
                d2d_context_->DrawBitmap(
                    document_bitmap_.get(),
                    destination,
                    1.0F,
                    D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
            }
        }

        void DrawSelectionOutline()
        {
            if (selection_segments_.empty() || !selection_black_ || !selection_white_ ||
                document_rect_.width <= 0.0F || document_rect_.height <= 0.0F)
            {
                return;
            }

            auto const dpi_scale = rasterization_scale_;
            D2D1_RECT_F const clip{
                document_rect_.x * dpi_scale,
                document_rect_.y * dpi_scale,
                (document_rect_.x + document_rect_.width) * dpi_scale,
                (document_rect_.y + document_rect_.height) * dpi_scale,
            };
            auto const x_scale = (clip.right - clip.left) / static_cast<float>(document_width_);
            auto const y_scale = (clip.bottom - clip.top) / static_cast<float>(document_height_);
            auto const previous_antialias_mode = d2d_context_->GetAntialiasMode();
            d2d_context_->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
            d2d_context_->PushAxisAlignedClip(clip, D2D1_ANTIALIAS_MODE_ALIASED);

            for (auto const& source : selection_segments_)
            {
                auto const horizontal = source.y1 == source.y2;
                auto const fixed_coordinate = horizontal ? source.y1 : source.x1;
                auto const fixed_limit = horizontal
                    ? static_cast<float>(document_height_)
                    : static_cast<float>(document_width_);
                if (fixed_coordinate < 0.0F || fixed_coordinate > fixed_limit)
                {
                    continue;
                }

                auto const variable_limit = horizontal
                    ? static_cast<float>(document_width_)
                    : static_cast<float>(document_height_);
                auto first = std::clamp(horizontal ? source.x1 : source.y1, 0.0F, variable_limit);
                auto second = std::clamp(horizontal ? source.x2 : source.y2, 0.0F, variable_limit);
                if (first == second)
                {
                    continue;
                }

                if (second < first)
                {
                    std::swap(first, second);
                }

                D2D1_POINT_2F start{};
                D2D1_POINT_2F end{};
                if (horizontal)
                {
                    start = D2D1::Point2F(clip.left + first * x_scale, clip.top + fixed_coordinate * y_scale);
                    end = D2D1::Point2F(clip.left + second * x_scale, start.y);
                }
                else
                {
                    start = D2D1::Point2F(clip.left + fixed_coordinate * x_scale, clip.top + first * y_scale);
                    end = D2D1::Point2F(start.x, clip.top + second * y_scale);
                }
                DrawSelectionSegment(start, end);
            }

            d2d_context_->PopAxisAlignedClip();
            d2d_context_->SetAntialiasMode(previous_antialias_mode);
        }

        void DrawSelectionSegment(D2D1_POINT_2F const start, D2D1_POINT_2F const end)
        {
            auto const delta_x = end.x - start.x;
            auto const delta_y = end.y - start.y;
            auto const length = std::abs(delta_x) + std::abs(delta_y);
            if (length <= 0.0F)
            {
                return;
            }

            d2d_context_->DrawLine(start, end, selection_black_.get(), 1.0F);
            auto const unit_x = delta_x / length;
            auto const unit_y = delta_y / length;

            // Anchor the dash pattern to absolute device-pixel coordinates,
            // rather than restarting it at each submitted edge. Application
            // snapshots commonly contain one edge per selected pixel; using a
            // shared screen-space phase keeps adjacent and overlapping edges
            // visually continuous without coupling the renderer to the mask.
            auto const absolute_start = std::abs(delta_x) >= std::abs(delta_y)
                ? start.x
                : start.y;
            auto cycle_position = std::fmod(
                absolute_start + selection_phase_,
                SelectionDashPeriod);
            if (cycle_position < 0.0F)
            {
                cycle_position += SelectionDashPeriod;
            }
            auto distance = -cycle_position;
            while (distance + SelectionDashLength <= 0.0F)
            {
                distance += SelectionDashPeriod;
            }
            for (; distance < length; distance += SelectionDashPeriod)
            {
                auto const dash_start = (std::max)(0.0F, distance);
                auto const dash_end = (std::min)(length, distance + SelectionDashLength);
                if (dash_end <= dash_start)
                {
                    continue;
                }
                d2d_context_->DrawLine(
                    D2D1::Point2F(start.x + unit_x * dash_start, start.y + unit_y * dash_start),
                    D2D1::Point2F(start.x + unit_x * dash_end, start.y + unit_y * dash_end),
                    selection_white_.get(),
                    1.0F);
            }
        }

        void UpdateDocumentRect() noexcept
        {
            if (document_width_ == 0 || document_height_ == 0 ||
                logical_width_ <= 0.0F || logical_height_ <= 0.0F)
            {
                document_rect_ = {};
                return;
            }

            auto const fit_scale = (std::min)(
                logical_width_ / static_cast<float>(document_width_),
                logical_height_ / static_cast<float>(document_height_));
            document_rect_.width = static_cast<float>(document_width_) * fit_scale;
            document_rect_.height = static_cast<float>(document_height_) * fit_scale;
            document_rect_.x = (logical_width_ - document_rect_.width) * 0.5F;
            document_rect_.y = (logical_height_ - document_rect_.height) * 0.5F;
        }

        void RecoverDevice()
        {
            CreateDeviceResources();
            CreateOrResizeSwapChain();
        }

        winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel panel_{ nullptr };
        winrt::com_ptr<ID3D11Device> d3d_device_;
        winrt::com_ptr<ID3D11DeviceContext> d3d_context_;
        winrt::com_ptr<ID2D1Factory1> d2d_factory_;
        winrt::com_ptr<ID2D1Device> d2d_device_;
        winrt::com_ptr<ID2D1DeviceContext> d2d_context_;
        winrt::com_ptr<IDXGISwapChain1> swap_chain_;
        winrt::com_ptr<ID2D1Bitmap1> target_bitmap_;
        winrt::com_ptr<ID2D1Bitmap1> document_bitmap_;
        winrt::com_ptr<ID2D1SolidColorBrush> checker_light_;
        winrt::com_ptr<ID2D1SolidColorBrush> checker_dark_;
        winrt::com_ptr<ID2D1SolidColorBrush> selection_black_;
        winrt::com_ptr<ID2D1SolidColorBrush> selection_white_;

        std::vector<std::byte> document_pixels_;
        std::vector<SelectionEdgeSegment> selection_segments_;
        std::uint32_t document_width_{};
        std::uint32_t document_height_{};
        std::uint32_t document_stride_{};
        std::uint32_t pixel_width_{};
        std::uint32_t pixel_height_{};
        float logical_width_{};
        float logical_height_{};
        float rasterization_scale_{ 1.0F };
        float selection_phase_{};
        CanvasDocumentRect document_rect_{};
    };

    D3DCanvasRenderer::D3DCanvasRenderer() : impl_(std::make_unique<Impl>()) {}
    D3DCanvasRenderer::~D3DCanvasRenderer() = default;
    D3DCanvasRenderer::D3DCanvasRenderer(D3DCanvasRenderer&&) noexcept = default;
    D3DCanvasRenderer& D3DCanvasRenderer::operator=(D3DCanvasRenderer&&) noexcept = default;

    void D3DCanvasRenderer::Attach(winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel const& panel)
    {
        impl_->Attach(panel);
    }

    void D3DCanvasRenderer::Resize(
        float const logical_width,
        float const logical_height,
        float const rasterization_scale)
    {
        impl_->Resize(logical_width, logical_height, rasterization_scale);
    }

    void D3DCanvasRenderer::SetDocument(DocumentBitmapView const& document)
    {
        impl_->SetDocument(document);
    }

    void D3DCanvasRenderer::ClearDocument() noexcept
    {
        impl_->ClearDocument();
    }

    void D3DCanvasRenderer::SetSelectionOutline(std::span<SelectionEdgeSegment const> const segments)
    {
        impl_->SetSelectionOutline(segments);
    }

    void D3DCanvasRenderer::ClearSelectionOutline() noexcept
    {
        impl_->ClearSelectionOutline();
    }

    void D3DCanvasRenderer::SetSelectionAnimationPhase(float const phase_pixels) noexcept
    {
        impl_->SetSelectionAnimationPhase(phase_pixels);
    }

    void D3DCanvasRenderer::AdvanceSelectionAnimationPhase(float const delta_pixels) noexcept
    {
        impl_->AdvanceSelectionAnimationPhase(delta_pixels);
    }

    float D3DCanvasRenderer::SelectionAnimationPhase() const noexcept
    {
        return impl_->SelectionAnimationPhase();
    }

    bool D3DCanvasRenderer::Render()
    {
        return impl_->Render();
    }

    bool D3DCanvasRenderer::Render(DocumentBitmapView const& document)
    {
        impl_->SetDocument(document);
        return impl_->Render();
    }

    std::optional<DocumentPixelPoint> D3DCanvasRenderer::TryMapPanelToDocument(
        float const panel_x,
        float const panel_y) const noexcept
    {
        return impl_->TryMapPanelToDocument(panel_x, panel_y);
    }

    CanvasDocumentRect D3DCanvasRenderer::DocumentRectInPanel() const noexcept
    {
        return impl_->DocumentRectInPanel();
    }
}
