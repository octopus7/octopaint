#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>

#include <winrt/Microsoft.UI.Xaml.Controls.h>

namespace octopaint::winui
{
    struct DocumentBitmapView final
    {
        std::uint32_t width{};
        std::uint32_t height{};
        std::uint32_t stride{};
        std::span<std::byte const> premultiplied_bgra{};
    };

    struct DocumentPixelPoint final
    {
        std::uint32_t x{};
        std::uint32_t y{};
    };

    struct CanvasDocumentRect final
    {
        float x{};
        float y{};
        float width{};
        float height{};
    };

    // A selection boundary edge expressed in document pixel coordinates.
    // Endpoints must form a horizontal or vertical segment. Coordinates refer
    // to pixel boundaries, so a full-document outline spans (0, 0) to
    // (document width, document height).
    struct SelectionEdgeSegment final
    {
        float x1{};
        float y1{};
        float x2{};
        float y2{};
    };

    // Owns the Direct3D/Direct2D resources associated with one SwapChainPanel.
    // Call this object only from the UI thread that owns the attached panel.
    class D3DCanvasRenderer final
    {
    public:
        D3DCanvasRenderer();
        ~D3DCanvasRenderer();

        D3DCanvasRenderer(D3DCanvasRenderer const&) = delete;
        D3DCanvasRenderer& operator=(D3DCanvasRenderer const&) = delete;
        D3DCanvasRenderer(D3DCanvasRenderer&&) noexcept;
        D3DCanvasRenderer& operator=(D3DCanvasRenderer&&) noexcept;

        void Attach(winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel const& panel);
        void Resize(float logical_width, float logical_height, float rasterization_scale = 1.0F);

        void SetDocument(DocumentBitmapView const& document);
        void ClearDocument() noexcept;
        void SetSelectionOutline(std::span<SelectionEdgeSegment const> segments);
        void ClearSelectionOutline() noexcept;
        void SetSelectionAnimationPhase(float phase_pixels) noexcept;
        void AdvanceSelectionAnimationPhase(float delta_pixels) noexcept;
        [[nodiscard]] float SelectionAnimationPhase() const noexcept;

        [[nodiscard]] bool Render();
        [[nodiscard]] bool Render(DocumentBitmapView const& document);

        [[nodiscard]] std::optional<DocumentPixelPoint> TryMapPanelToDocument(
            float panel_x,
            float panel_y) const noexcept;
        [[nodiscard]] CanvasDocumentRect DocumentRectInPanel() const noexcept;

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };
}
