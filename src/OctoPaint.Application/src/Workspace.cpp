#include <octopaint/application/Workspace.h>

#include <octopaint/core/Compositor.h>
#include <octopaint/core/Document.h>
#include <octopaint/core/Layer.h>
#include <octopaint/core/Tools.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <format>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <utility>

namespace
{
    std::atomic_uint64_t next_document_id{ 1 };
    std::atomic_uint64_t next_layer_id{ 1 };

    [[nodiscard]] std::uint64_t AllocateUniqueId(std::atomic_uint64_t& source, char const* const exhausted_message)
    {
        auto value = source.load(std::memory_order_relaxed);
        for (;;)
        {
            // Leave max as a permanent exhausted sentinel instead of allowing
            // unsigned wraparound to make a stale identity valid again.
            if (value == 0 || value == std::numeric_limits<std::uint64_t>::max())
            {
                throw std::overflow_error(exhausted_message);
            }

            if (source.compare_exchange_weak(
                value,
                value + 1,
                std::memory_order_relaxed,
                std::memory_order_relaxed))
            {
                return value;
            }
        }
    }

    [[nodiscard]] std::uint64_t AllocateDocumentId()
    {
        return AllocateUniqueId(next_document_id, "The process exhausted its document identities.");
    }

    [[nodiscard]] std::uint64_t AllocateLayerId()
    {
        return AllocateUniqueId(next_layer_id, "The process exhausted its layer identities.");
    }
}

namespace octopaint::application::detail
{
    struct LayerDocumentState final
    {
        core::LayerTree tree;
        std::optional<LayerId> active_layer_id;
        core::SelectionMask selection;
    };

    struct LayerLocation final
    {
        std::optional<core::LayerId> parent;
        std::size_t index{};
    };

    [[nodiscard]] core::SparseTileStore MakeOpaqueWhiteCanvasTiles(CanvasSize const size)
    {
        std::vector<std::byte> white_bytes(
            core::Rgba8TileByteCount,
            static_cast<std::byte>(0xff));
        auto const white_tile = core::TilePayload::FromRgba8(white_bytes);

        core::SparseTileStore tiles;
        auto const columns = (size.width - 1) / core::TileExtent + 1;
        auto const rows = (size.height - 1) / core::TileExtent + 1;
        for (std::uint32_t y = 0; y < rows; ++y)
        {
            for (std::uint32_t x = 0; x < columns; ++x)
            {
                [[maybe_unused]] auto const result = tiles.Publish({
                    static_cast<std::int32_t>(x),
                    static_cast<std::int32_t>(y),
                    0 }, white_tile);
            }
        }
        return tiles;
    }

    [[nodiscard]] core::LayerId ToCore(LayerId const id) noexcept
    {
        return core::LayerId{ id.Value() };
    }

    [[nodiscard]] core::Layer* Find(LayerDocumentState& state, LayerId const id) noexcept
    {
        return state.tree.Find(ToCore(id));
    }

    [[nodiscard]] core::Layer const* Find(LayerDocumentState const& state, LayerId const id) noexcept
    {
        return state.tree.Find(ToCore(id));
    }

    [[nodiscard]] std::optional<LayerLocation> LocateIn(
        std::span<std::unique_ptr<core::Layer> const> const layers,
        core::LayerId const id,
        std::optional<core::LayerId> const parent = std::nullopt) noexcept
    {
        for (std::size_t index = 0; index < layers.size(); ++index)
        {
            auto const& layer = layers[index];
            if (layer->Properties().id == id)
            {
                return LayerLocation{ parent, index };
            }
            if (layer->Kind() == core::LayerKind::Group)
            {
                auto const& group = static_cast<core::GroupLayer const&>(*layer);
                if (auto const found = LocateIn(group.Children(), id, layer->Properties().id))
                {
                    return found;
                }
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<LayerLocation> Locate(
        LayerDocumentState const& state,
        LayerId const id) noexcept
    {
        return LocateIn(state.tree.Roots(), ToCore(id));
    }

    [[nodiscard]] std::size_t ChildCount(
        LayerDocumentState const& state,
        std::optional<LayerId> const parent)
    {
        if (!parent)
        {
            return state.tree.Roots().size();
        }

        auto const* const layer = Find(state, *parent);
        if (!layer)
        {
            throw std::out_of_range("The parent layer does not exist.");
        }
        if (layer->Kind() != core::LayerKind::Group)
        {
            throw std::invalid_argument("Only a group layer can contain child layers.");
        }
        return static_cast<core::GroupLayer const*>(layer)->Children().size();
    }

    void RequireSuccess(core::LayerMutationResult const result)
    {
        if (!result)
        {
            throw std::invalid_argument(std::string{ core::LayerValidationMessage(result.code) });
        }
    }

    [[nodiscard]] bool SelectionMasksEqual(
        core::SelectionMask const& left,
        core::SelectionMask const& right) noexcept
    {
        return left.Bounds() == right.Bounds()
            && std::ranges::equal(left.Coverage(), right.Coverage());
    }

    struct CanvasPixelBuffer final
    {
        std::size_t row_stride{};
        std::vector<std::byte> pixels;
    };

    [[nodiscard]] CanvasPixelBuffer MakeCanvasPixelBuffer(core::CanvasSize const size)
    {
        auto const width = static_cast<std::size_t>(size.width);
        auto const height = static_cast<std::size_t>(size.height);
        if (width > std::numeric_limits<std::size_t>::max() / core::Rgba8BytesPerPixel)
        {
            throw std::length_error("Composite snapshot row stride exceeds addressable memory.");
        }
        auto const row_stride = width * core::Rgba8BytesPerPixel;
        if (height != 0 && row_stride > std::numeric_limits<std::size_t>::max() / height)
        {
            throw std::length_error("Composite snapshot exceeds addressable memory.");
        }
        return {
            .row_stride = row_stride,
            .pixels = std::vector<std::byte>(row_stride * height)
        };
    }

    [[nodiscard]] CanvasPixelBuffer SnapshotRasterPixels(
        core::RasterLayer const& layer,
        core::CanvasSize const size)
    {
        auto snapshot = MakeCanvasPixelBuffer(size);
        for (auto const key : layer.Tiles().Keys())
        {
            if (key.level != 0)
            {
                continue;
            }
            auto const payload = layer.Tiles().Read(key);
            if (!payload)
            {
                continue;
            }
            auto const source = payload->Pixels();
            auto const origin_x = static_cast<std::int64_t>(key.x) * core::TileExtent;
            auto const origin_y = static_cast<std::int64_t>(key.y) * core::TileExtent;
            for (std::uint32_t local_y = 0; local_y < core::TileExtent; ++local_y)
            {
                auto const canvas_y = origin_y + local_y;
                if (canvas_y < 0 || canvas_y >= static_cast<std::int64_t>(size.height))
                {
                    continue;
                }
                for (std::uint32_t local_x = 0; local_x < core::TileExtent; ++local_x)
                {
                    auto const canvas_x = origin_x + local_x;
                    if (canvas_x < 0 || canvas_x >= static_cast<std::int64_t>(size.width))
                    {
                        continue;
                    }
                    auto const source_index =
                        (static_cast<std::size_t>(local_y) * core::TileExtent + local_x)
                        * core::Rgba8BytesPerPixel;
                    auto const destination_index = static_cast<std::size_t>(canvas_y) * snapshot.row_stride
                        + static_cast<std::size_t>(canvas_x) * core::Rgba8BytesPerPixel;
                    snapshot.pixels[destination_index] = source[source_index + 2];
                    snapshot.pixels[destination_index + 1] = source[source_index + 1];
                    snapshot.pixels[destination_index + 2] = source[source_index];
                    snapshot.pixels[destination_index + 3] = source[source_index + 3];
                }
            }
        }
        return snapshot;
    }

    void CompositeLayers(
        std::span<std::unique_ptr<core::Layer> const> const layers,
        core::CanvasSize const size,
        CanvasPixelBuffer& destination)
    {
        // LayerTree order is back-to-front: later siblings are painted over
        // earlier siblings. Groups are isolated into a transparent buffer so
        // group visibility and opacity apply to their flattened contribution.
        for (auto const& layer : layers)
        {
            auto const& properties = layer->Properties();
            if (!properties.visible || properties.opacity <= 0.0F)
            {
                continue;
            }

            CanvasPixelBuffer source;
            if (layer->Kind() == core::LayerKind::Raster)
            {
                source = SnapshotRasterPixels(static_cast<core::RasterLayer const&>(*layer), size);
            }
            else
            {
                source = MakeCanvasPixelBuffer(size);
                CompositeLayers(
                    static_cast<core::GroupLayer const&>(*layer).Children(),
                    size,
                    source);
            }

            auto const result = core::CompositePremultipliedBgra8(
                destination.pixels,
                destination.row_stride,
                source.pixels,
                source.row_stride,
                size.width,
                size.height,
                properties.opacity,
                properties.blend_mode);
            if (result != core::CompositeResult::Succeeded)
            {
                throw std::runtime_error(std::string{ core::CompositeResultMessage(result) });
            }
        }
    }
}

namespace octopaint::application
{
    class PaintStrokeCommand final : public DocumentCommand
    {
    public:
        PaintStrokeCommand(PaintStrokeRequest request, CanvasSize const canvas_size)
            : request_(std::move(request)), canvas_size_(canvas_size)
        {
        }

        [[nodiscard]] std::string Label() const override
        {
            return request_.tool == PaintTool::Pencil ? "Pencil stroke" : "Airbrush stroke";
        }

        void Execute(DocumentMutation& document) override
        {
            auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
            auto* const layer = detail::Find(state, request_.layer_id);
            if (!layer || layer->Kind() != core::LayerKind::Raster)
            {
                throw std::logic_error("The paint target is no longer a raster layer.");
            }

            auto& tiles = static_cast<core::RasterLayer*>(layer)->Tiles();
            if (after_)
            {
                tiles = *after_;
                return;
            }

            before_ = tiles;
            auto candidate = tiles;
            auto pixels = Rasterize();
            pixels.erase(std::remove_if(pixels.begin(), pixels.end(), [this](core::PaintPixel const& pixel)
            {
                return pixel.position.x < 0 || pixel.position.y < 0
                    || static_cast<std::uint64_t>(pixel.position.x) >= canvas_size_.width
                    || static_cast<std::uint64_t>(pixel.position.y) >= canvas_size_.height;
            }), pixels.end());

            // An empty selection means the entire canvas is editable. Current
            // selection rasterizers expose binary 0/1 coverage, so retain only
            // pixels selected by the document mask.
            if (!state.selection.Empty())
            {
                pixels.erase(std::remove_if(pixels.begin(), pixels.end(), [&state](core::PaintPixel const& pixel)
                {
                    return state.selection.CoverageAt(pixel.position) == 0;
                }), pixels.end());
            }

            if (!pixels.empty())
            {
                auto min_x = pixels.front().position.x;
                auto max_x = min_x;
                auto min_y = pixels.front().position.y;
                auto max_y = min_y;
                for (auto const& pixel : pixels)
                {
                    min_x = std::min(min_x, pixel.position.x);
                    max_x = std::max(max_x, pixel.position.x);
                    min_y = std::min(min_y, pixel.position.y);
                    max_y = std::max(max_y, pixel.position.y);
                }
                changed_bounds_ = PixelBounds{
                    min_x,
                    min_y,
                    static_cast<std::uint32_t>(static_cast<std::int64_t>(max_x) - min_x + 1),
                    static_cast<std::uint32_t>(static_cast<std::int64_t>(max_y) - min_y + 1)
                };
                core::ApplyPaintPixels(
                    candidate,
                    pixels,
                    { .alpha_locked = layer->Properties().alpha_locked });
            }

            changed_ = !TileStoresEqual(*before_, candidate);
            if (!changed_)
            {
                changed_bounds_.reset();
                return;
            }
            after_ = std::move(candidate);
            tiles = *after_;
        }

        void Undo(DocumentMutation& document) override
        {
            if (!changed_ || !before_)
            {
                throw std::logic_error("Cannot undo a paint stroke that did not change pixels.");
            }
            auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
            auto* const layer = detail::Find(state, request_.layer_id);
            if (!layer || layer->Kind() != core::LayerKind::Raster)
            {
                throw std::logic_error("The paint target is no longer a raster layer.");
            }
            static_cast<core::RasterLayer*>(layer)->Tiles() = *before_;
        }

        [[nodiscard]] bool Changed() const noexcept { return changed_; }
        [[nodiscard]] std::optional<PixelBounds> ChangedBounds() const noexcept { return changed_bounds_; }

    private:
        [[nodiscard]] static bool TileStoresEqual(
            core::SparseTileStore const& left,
            core::SparseTileStore const& right)
        {
            auto const left_keys = left.Keys();
            auto const right_keys = right.Keys();
            if (left_keys != right_keys)
            {
                return false;
            }
            for (auto const key : left_keys)
            {
                auto const left_tile = left.Read(key);
                auto const right_tile = right.Read(key);
                if (!left_tile || !right_tile
                    || !std::ranges::equal(left_tile->Pixels(), right_tile->Pixels()))
                {
                    return false;
                }
            }
            return true;
        }

        struct ProcessedSample final
        {
            core::StylusSample sample;
            double elapsed_seconds{};
            bool begins_new_segment{};
        };

        [[nodiscard]] std::vector<ProcessedSample> StabilizedSamples() const
        {
            core::PressureMapper pressure_mapper({
                request_.pressure.minimum_input,
                request_.pressure.maximum_input,
                request_.pressure.gamma
            });
            core::StrokeStabilizer stabilizer({
                request_.stabilizer.enabled,
                request_.stabilizer.strength
            });

            std::vector<ProcessedSample> samples;
            samples.reserve(request_.samples.size() * 2);
            for (std::size_t index = 0; index < request_.samples.size(); ++index)
            {
                auto const& input = request_.samples[index];
                if (!std::isfinite(input.x) || !std::isfinite(input.y)
                    || input.x < static_cast<double>(std::numeric_limits<std::int32_t>::min())
                    || input.x > static_cast<double>(std::numeric_limits<std::int32_t>::max())
                    || input.y < static_cast<double>(std::numeric_limits<std::int32_t>::min())
                    || input.y > static_cast<double>(std::numeric_limits<std::int32_t>::max())
                    || !std::isfinite(input.elapsed_seconds) || input.elapsed_seconds < 0.0)
                {
                    throw std::invalid_argument("Paint samples must have finite coordinates and non-negative elapsed time.");
                }

                auto const starts_segment = index == 0 || input.begins_new_segment;
                if (index > 0 && input.begins_new_segment)
                {
                    if (auto const endpoint = stabilizer.Flush())
                    {
                        samples.push_back({ *endpoint, 0.0, false });
                    }
                }
                samples.push_back({ stabilizer.Push({
                    {
                        static_cast<std::int32_t>(std::llround(input.x)),
                        static_cast<std::int32_t>(std::llround(input.y))
                    },
                    pressure_mapper.Map(input.pressure)
                }), starts_segment ? 0.0 : input.elapsed_seconds, starts_segment });
            }
            if (auto const endpoint = stabilizer.Flush())
            {
                samples.push_back({ *endpoint, 0.0, false });
            }
            return samples;
        }

        [[nodiscard]] std::vector<core::PaintPixel> Rasterize() const
        {
            auto const samples = StabilizedSamples();
            if (samples.empty())
            {
                return {};
            }
            core::Rgba8 const color{
                request_.color.red,
                request_.color.green,
                request_.color.blue,
                request_.color.alpha
            };
            std::vector<core::PaintPixel> pixels;
            if (request_.tool == PaintTool::Pencil)
            {
                for (std::size_t index = 0; index < samples.size(); ++index)
                {
                    auto segment = core::RasterizePencilSamples(
                        samples[index].begins_new_segment ? samples[index].sample : samples[index - 1].sample,
                        samples[index].sample,
                        color);
                    if (!samples[index].begins_new_segment && !segment.empty())
                    {
                        segment.erase(segment.begin());
                    }
                    auto const pressure_opacity = request_.brush.pressure_affects_opacity
                        ? samples[index].sample.pressure : 1.0F;
                    for (auto& pixel : segment)
                    {
                        pixel.opacity = request_.brush.opacity * pressure_opacity;
                    }
                    pixels.insert(pixels.end(), segment.begin(), segment.end());
                }
                return pixels;
            }

            core::AirbrushAccumulator accumulator({
                .radius = request_.brush.radius,
                .flow_per_second = request_.brush.flow_per_second,
                .fixed_timestep_seconds = request_.brush.fixed_timestep_seconds,
                .hardness = request_.brush.hardness,
                .spacing = request_.brush.spacing,
                .opacity = request_.brush.opacity,
                .pressure_affects_size = request_.brush.pressure_affects_size,
                .pressure_affects_opacity = request_.brush.pressure_affects_opacity
            });
            for (std::size_t index = 0; index < samples.size(); ++index)
            {
                if (samples[index].begins_new_segment)
                {
                    accumulator.Reset();
                }
                auto dabs = accumulator.Advance(
                    samples[index].begins_new_segment ? samples[index].sample : samples[index - 1].sample,
                    samples[index].sample,
                    samples[index].elapsed_seconds,
                    color);
                auto dab_pixels = core::RasterizeDabs(dabs);
                pixels.insert(pixels.end(), dab_pixels.begin(), dab_pixels.end());
            }
            return pixels;
        }

        PaintStrokeRequest request_;
        CanvasSize canvas_size_;
        std::optional<core::SparseTileStore> before_;
        std::optional<core::SparseTileStore> after_;
        std::optional<PixelBounds> changed_bounds_;
        bool changed_{};
    };

    class SelectionCommand final : public DocumentCommand
    {
    public:
        explicit SelectionCommand(core::SelectionMask selection)
            : selection_(std::move(selection))
        {
        }

        [[nodiscard]] std::string Label() const override
        {
            return "Set selection";
        }

        void Execute(DocumentMutation& document) override
        {
            auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
            if (!captured_previous_)
            {
                previous_ = state.selection;
                captured_previous_ = true;
            }
            state.selection = selection_;
        }

        void Undo(DocumentMutation& document) override
        {
            if (!captured_previous_)
            {
                throw std::logic_error("Cannot undo a selection that has not executed.");
            }
            auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
            state.selection = previous_;
        }

    private:
        core::SelectionMask selection_;
        core::SelectionMask previous_;
        bool captured_previous_{};
    };

    struct Workspace::Impl final
    {
        struct HistoryEntry final
        {
            std::unique_ptr<DocumentCommand> command;
            std::uint64_t before_revision{};
            std::uint64_t after_revision{};
        };

        struct OpenDocument final
        {
            DocumentId id;
            core::Document document;
            detail::LayerDocumentState layers;
            std::vector<HistoryEntry> history;
            std::size_t history_position{};
            std::uint64_t current_revision{};
            std::uint64_t saved_revision{};
            std::uint64_t next_revision{ 1 };
        };

        std::vector<OpenDocument> documents;
        std::optional<DocumentId> active_document_id;

        [[nodiscard]] OpenDocument* Find(DocumentId const id) noexcept
        {
            auto const iterator = std::ranges::find(documents, id, &OpenDocument::id);
            return iterator == documents.end() ? nullptr : &*iterator;
        }

        [[nodiscard]] OpenDocument const* Find(DocumentId const id) const noexcept
        {
            auto const iterator = std::ranges::find(documents, id, &OpenDocument::id);
            return iterator == documents.end() ? nullptr : &*iterator;
        }

        [[nodiscard]] OpenDocument* Active() noexcept
        {
            return active_document_id ? Find(*active_document_id) : nullptr;
        }

        [[nodiscard]] OpenDocument const* Active() const noexcept
        {
            return active_document_id ? Find(*active_document_id) : nullptr;
        }
    };

    DocumentMutation::DocumentMutation(void* const document, void* const layers) noexcept
        : document_(document), layers_(layers)
    {
    }

    std::string const& DocumentMutation::Title() const noexcept
    {
        return static_cast<core::Document const*>(document_)->Title();
    }

    CanvasSize DocumentMutation::Size() const noexcept
    {
        auto const size = static_cast<core::Document const*>(document_)->Size();
        return { .width = size.width, .height = size.height };
    }

    void DocumentMutation::Rename(std::string title)
    {
        static_cast<core::Document*>(document_)->Rename(std::move(title));
    }

    RenameDocumentCommand::RenameDocumentCommand(std::string new_title)
        : new_title_(std::move(new_title))
    {
        if (new_title_.empty())
        {
            throw std::invalid_argument("A document title cannot be empty.");
        }
    }

    std::string RenameDocumentCommand::Label() const
    {
        return "Rename document";
    }

    void RenameDocumentCommand::Execute(DocumentMutation& document)
    {
        if (!captured_previous_title_)
        {
            previous_title_ = document.Title();
            captured_previous_title_ = true;
        }

        document.Rename(new_title_);
    }

    void RenameDocumentCommand::Undo(DocumentMutation& document)
    {
        if (!captured_previous_title_)
        {
            throw std::logic_error("Cannot undo a command that has not executed.");
        }

        document.Rename(previous_title_);
    }

    AddRasterLayerCommand::AddRasterLayerCommand(
        std::string name,
        std::optional<LayerId> parent,
        std::optional<std::size_t> index)
        : id_(LayerId{ AllocateLayerId() }),
          name_(std::move(name)),
          parent_(parent),
          requested_index_(index)
    {
        if (name_.empty())
        {
            throw std::invalid_argument("A layer name cannot be empty.");
        }
    }

    LayerId AddRasterLayerCommand::CreatedLayerId() const noexcept
    {
        return id_;
    }

    std::string AddRasterLayerCommand::Label() const
    {
        return "Add raster layer";
    }

    void AddRasterLayerCommand::Execute(DocumentMutation& document)
    {
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        if (!executed_once_)
        {
            previous_active_layer_ = state.active_layer_id;
            insertion_index_ = requested_index_.value_or(detail::ChildCount(state, parent_));
            executed_once_ = true;
        }

        auto layer = std::make_unique<core::RasterLayer>(core::LayerProperties{
            .id = detail::ToCore(id_),
            .name_utf8 = name_
        });
        detail::RequireSuccess(state.tree.Insert(
            parent_ ? std::optional{ detail::ToCore(*parent_) } : std::nullopt,
            insertion_index_,
            std::move(layer)));
        state.active_layer_id = id_;
    }

    void AddRasterLayerCommand::Undo(DocumentMutation& document)
    {
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        auto removed = state.tree.Remove(detail::ToCore(id_));
        detail::RequireSuccess(removed.result);
        state.active_layer_id = previous_active_layer_;
    }

    AddGroupLayerCommand::AddGroupLayerCommand(
        std::string name,
        std::optional<LayerId> parent,
        std::optional<std::size_t> index)
        : id_(LayerId{ AllocateLayerId() }),
          name_(std::move(name)),
          parent_(parent),
          requested_index_(index)
    {
        if (name_.empty())
        {
            throw std::invalid_argument("A layer name cannot be empty.");
        }
    }

    LayerId AddGroupLayerCommand::CreatedLayerId() const noexcept
    {
        return id_;
    }

    std::string AddGroupLayerCommand::Label() const
    {
        return "Add group layer";
    }

    void AddGroupLayerCommand::Execute(DocumentMutation& document)
    {
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        if (!executed_once_)
        {
            previous_active_layer_ = state.active_layer_id;
            insertion_index_ = requested_index_.value_or(detail::ChildCount(state, parent_));
            executed_once_ = true;
        }

        auto layer = std::make_unique<core::GroupLayer>(core::LayerProperties{
            .id = detail::ToCore(id_),
            .name_utf8 = name_
        });
        detail::RequireSuccess(state.tree.Insert(
            parent_ ? std::optional{ detail::ToCore(*parent_) } : std::nullopt,
            insertion_index_,
            std::move(layer)));
        state.active_layer_id = id_;
    }

    void AddGroupLayerCommand::Undo(DocumentMutation& document)
    {
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        auto removed = state.tree.Remove(detail::ToCore(id_));
        detail::RequireSuccess(removed.result);
        state.active_layer_id = previous_active_layer_;
    }

    struct RemoveLayerCommand::Impl final
    {
        explicit Impl(LayerId const layer_id)
            : id(layer_id)
        {
        }

        LayerId id;
        std::optional<core::LayerId> parent;
        std::size_t index{};
        std::optional<LayerId> previous_active_layer;
        std::unique_ptr<core::Layer> removed_layer;
        bool executed_once{};
    };

    RemoveLayerCommand::RemoveLayerCommand(LayerId const id)
        : impl_(std::make_unique<Impl>(id))
    {
        if (!id)
        {
            throw std::invalid_argument("A layer ID must be non-zero.");
        }
    }

    RemoveLayerCommand::~RemoveLayerCommand() = default;

    std::string RemoveLayerCommand::Label() const
    {
        return "Remove layer";
    }

    void RemoveLayerCommand::Execute(DocumentMutation& document)
    {
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        auto const location = detail::Locate(state, impl_->id);
        if (!location)
        {
            throw std::out_of_range("The layer does not exist.");
        }
        if (!impl_->executed_once)
        {
            impl_->parent = location->parent;
            impl_->index = location->index;
            impl_->previous_active_layer = state.active_layer_id;
            impl_->executed_once = true;
        }

        auto removal = state.tree.Remove(detail::ToCore(impl_->id));
        detail::RequireSuccess(removal.result);
        impl_->removed_layer = std::move(removal.layer);

        if (state.active_layer_id && !detail::Find(state, *state.active_layer_id))
        {
            if (impl_->parent)
            {
                state.active_layer_id = LayerId{ impl_->parent->Value() };
            }
            else if (!state.tree.Roots().empty())
            {
                state.active_layer_id = LayerId{ state.tree.Roots().front()->Properties().id.Value() };
            }
            else
            {
                state.active_layer_id.reset();
            }
        }
    }

    void RemoveLayerCommand::Undo(DocumentMutation& document)
    {
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        if (!impl_->removed_layer)
        {
            throw std::logic_error("Cannot undo a layer removal that has not executed.");
        }
        detail::RequireSuccess(state.tree.Insert(
            impl_->parent,
            impl_->index,
            std::move(impl_->removed_layer)));
        state.active_layer_id = impl_->previous_active_layer;
    }

    MoveLayerCommand::MoveLayerCommand(
        LayerId const id,
        std::optional<LayerId> new_parent,
        std::size_t const new_index)
        : id_(id), new_parent_(new_parent), new_index_(new_index)
    {
        if (!id_)
        {
            throw std::invalid_argument("A layer ID must be non-zero.");
        }
    }

    std::string MoveLayerCommand::Label() const
    {
        return "Move layer";
    }

    void MoveLayerCommand::Execute(DocumentMutation& document)
    {
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        if (!captured_previous_location_)
        {
            auto const location = detail::Locate(state, id_);
            if (!location)
            {
                throw std::out_of_range("The layer does not exist.");
            }
            if (location->parent)
            {
                previous_parent_ = LayerId{ location->parent->Value() };
            }
            previous_index_ = location->index;
            captured_previous_location_ = true;
        }

        detail::RequireSuccess(state.tree.Move(
            detail::ToCore(id_),
            new_parent_ ? std::optional{ detail::ToCore(*new_parent_) } : std::nullopt,
            new_index_));
    }

    void MoveLayerCommand::Undo(DocumentMutation& document)
    {
        if (!captured_previous_location_)
        {
            throw std::logic_error("Cannot undo a layer move that has not executed.");
        }
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        detail::RequireSuccess(state.tree.Move(
            detail::ToCore(id_),
            previous_parent_ ? std::optional{ detail::ToCore(*previous_parent_) } : std::nullopt,
            previous_index_));
    }

    RenameLayerCommand::RenameLayerCommand(LayerId const id, std::string new_name)
        : id_(id), new_name_(std::move(new_name))
    {
        if (!id_)
        {
            throw std::invalid_argument("A layer ID must be non-zero.");
        }
        if (new_name_.empty())
        {
            throw std::invalid_argument("A layer name cannot be empty.");
        }
    }

    std::string RenameLayerCommand::Label() const
    {
        return "Rename layer";
    }

    void RenameLayerCommand::Execute(DocumentMutation& document)
    {
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        auto* const layer = detail::Find(state, id_);
        if (!layer)
        {
            throw std::out_of_range("The layer does not exist.");
        }
        if (!captured_previous_name_)
        {
            previous_name_ = layer->Properties().name_utf8;
            captured_previous_name_ = true;
        }
        layer->Rename(new_name_);
    }

    void RenameLayerCommand::Undo(DocumentMutation& document)
    {
        if (!captured_previous_name_)
        {
            throw std::logic_error("Cannot undo a layer rename that has not executed.");
        }
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        auto* const layer = detail::Find(state, id_);
        if (!layer)
        {
            throw std::out_of_range("The layer does not exist.");
        }
        layer->Rename(previous_name_);
    }

    SetLayerOpacityCommand::SetLayerOpacityCommand(LayerId const id, float const opacity)
        : id_(id), opacity_(opacity)
    {
        if (!id_)
        {
            throw std::invalid_argument("A layer ID must be non-zero.");
        }
        if (!std::isfinite(opacity_) || opacity_ < 0.0F || opacity_ > 1.0F)
        {
            throw std::invalid_argument("Layer opacity must be finite and between zero and one.");
        }
    }

    std::string SetLayerOpacityCommand::Label() const
    {
        return "Set layer opacity";
    }

    void SetLayerOpacityCommand::Execute(DocumentMutation& document)
    {
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        auto* const layer = detail::Find(state, id_);
        if (!layer)
        {
            throw std::out_of_range("The layer does not exist.");
        }
        if (!captured_previous_opacity_)
        {
            previous_opacity_ = layer->Properties().opacity;
            captured_previous_opacity_ = true;
        }
        layer->SetOpacity(opacity_);
    }

    void SetLayerOpacityCommand::Undo(DocumentMutation& document)
    {
        if (!captured_previous_opacity_)
        {
            throw std::logic_error("Cannot undo an opacity command that has not executed.");
        }
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        auto* const layer = detail::Find(state, id_);
        if (!layer)
        {
            throw std::out_of_range("The layer does not exist.");
        }
        layer->SetOpacity(previous_opacity_);
    }

    SetLayerVisibilityCommand::SetLayerVisibilityCommand(LayerId const id, bool const visible)
        : id_(id), visible_(visible)
    {
        if (!id_)
        {
            throw std::invalid_argument("A layer ID must be non-zero.");
        }
    }

    std::string SetLayerVisibilityCommand::Label() const
    {
        return "Set layer visibility";
    }

    void SetLayerVisibilityCommand::Execute(DocumentMutation& document)
    {
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        auto* const layer = detail::Find(state, id_);
        if (!layer)
        {
            throw std::out_of_range("The layer does not exist.");
        }
        if (!captured_previous_visibility_)
        {
            previous_visibility_ = layer->Properties().visible;
            captured_previous_visibility_ = true;
        }
        layer->SetVisible(visible_);
    }

    void SetLayerVisibilityCommand::Undo(DocumentMutation& document)
    {
        if (!captured_previous_visibility_)
        {
            throw std::logic_error("Cannot undo a visibility command that has not executed.");
        }
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        auto* const layer = detail::Find(state, id_);
        if (!layer)
        {
            throw std::out_of_range("The layer does not exist.");
        }
        layer->SetVisible(previous_visibility_);
    }

    SetLayerLockedCommand::SetLayerLockedCommand(LayerId const id, bool const locked)
        : id_(id), locked_(locked)
    {
        if (!id_)
        {
            throw std::invalid_argument("A layer ID must be non-zero.");
        }
    }

    std::string SetLayerLockedCommand::Label() const
    {
        return "Set layer lock";
    }

    void SetLayerLockedCommand::Execute(DocumentMutation& document)
    {
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        auto* const layer = detail::Find(state, id_);
        if (!layer)
        {
            throw std::out_of_range("The layer does not exist.");
        }
        if (!captured_previous_locked_)
        {
            previous_locked_ = layer->Properties().locked;
            captured_previous_locked_ = true;
        }
        layer->SetLocked(locked_);
    }

    void SetLayerLockedCommand::Undo(DocumentMutation& document)
    {
        if (!captured_previous_locked_)
        {
            throw std::logic_error("Cannot undo a lock command that has not executed.");
        }
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        auto* const layer = detail::Find(state, id_);
        if (!layer)
        {
            throw std::out_of_range("The layer does not exist.");
        }
        layer->SetLocked(previous_locked_);
    }

    SetLayerAlphaLockedCommand::SetLayerAlphaLockedCommand(
        LayerId const id,
        bool const alpha_locked)
        : id_(id), alpha_locked_(alpha_locked)
    {
        if (!id_)
        {
            throw std::invalid_argument("A layer ID must be non-zero.");
        }
    }

    std::string SetLayerAlphaLockedCommand::Label() const
    {
        return "Set layer alpha lock";
    }

    void SetLayerAlphaLockedCommand::Execute(DocumentMutation& document)
    {
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        auto* const layer = detail::Find(state, id_);
        if (!layer)
        {
            throw std::out_of_range("The layer does not exist.");
        }
        if (!captured_previous_alpha_locked_)
        {
            previous_alpha_locked_ = layer->Properties().alpha_locked;
            captured_previous_alpha_locked_ = true;
        }
        layer->SetAlphaLocked(alpha_locked_);
    }

    void SetLayerAlphaLockedCommand::Undo(DocumentMutation& document)
    {
        if (!captured_previous_alpha_locked_)
        {
            throw std::logic_error("Cannot undo an alpha lock command that has not executed.");
        }
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        auto* const layer = detail::Find(state, id_);
        if (!layer)
        {
            throw std::out_of_range("The layer does not exist.");
        }
        layer->SetAlphaLocked(previous_alpha_locked_);
    }

    SetLayerBlendModeCommand::SetLayerBlendModeCommand(LayerId const id, BlendMode const blend_mode)
        : id_(id), blend_mode_(blend_mode)
    {
        if (!id_)
        {
            throw std::invalid_argument("A layer ID must be non-zero.");
        }
    }

    std::string SetLayerBlendModeCommand::Label() const
    {
        return "Set layer blend mode";
    }

    void SetLayerBlendModeCommand::Execute(DocumentMutation& document)
    {
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        auto* const layer = detail::Find(state, id_);
        if (!layer)
        {
            throw std::out_of_range("The layer does not exist.");
        }
        if (!captured_previous_blend_mode_)
        {
            previous_blend_mode_ = static_cast<BlendMode>(layer->Properties().blend_mode);
            captured_previous_blend_mode_ = true;
        }
        layer->SetBlendMode(static_cast<core::BlendMode>(blend_mode_));
    }

    void SetLayerBlendModeCommand::Undo(DocumentMutation& document)
    {
        if (!captured_previous_blend_mode_)
        {
            throw std::logic_error("Cannot undo a blend mode command that has not executed.");
        }
        auto& state = *static_cast<detail::LayerDocumentState*>(document.layers_);
        auto* const layer = detail::Find(state, id_);
        if (!layer)
        {
            throw std::out_of_range("The layer does not exist.");
        }
        layer->SetBlendMode(static_cast<core::BlendMode>(previous_blend_mode_));
    }

    Workspace::Workspace()
        : impl_(std::make_unique<Impl>())
    {
    }

    Workspace::~Workspace() = default;
    Workspace::Workspace(Workspace&&) noexcept = default;
    Workspace& Workspace::operator=(Workspace&&) noexcept = default;

    DocumentId Workspace::NewDocument(std::string title, CanvasSize const size)
    {
        if (!size.IsValid())
        {
            throw std::invalid_argument("Canvas dimensions must be greater than zero.");
        }

        DocumentId const id{ AllocateDocumentId() };
        LayerId const default_layer_id{ AllocateLayerId() };
        detail::LayerDocumentState layers;
        detail::RequireSuccess(layers.tree.AppendRoot(std::make_unique<core::RasterLayer>(
            core::LayerProperties{
                .id = detail::ToCore(default_layer_id),
                .name_utf8 = "Layer 1"
            },
            detail::MakeOpaqueWhiteCanvasTiles(size))));
        layers.active_layer_id = default_layer_id;
        impl_->documents.push_back({
            .id = id,
            .document = core::Document{
                std::move(title),
                core::CanvasSize{ .width = size.width, .height = size.height } },
            .layers = std::move(layers)
        });
        impl_->active_document_id = id;
        return id;
    }

    bool Workspace::CloseDocument(DocumentId const id)
    {
        auto const iterator = std::ranges::find(impl_->documents, id, &Impl::OpenDocument::id);
        if (iterator == impl_->documents.end())
        {
            return false;
        }

        bool const was_active = impl_->active_document_id == id;
        auto const index = static_cast<std::size_t>(iterator - impl_->documents.begin());
        impl_->documents.erase(iterator);

        if (was_active)
        {
            if (impl_->documents.empty())
            {
                impl_->active_document_id.reset();
            }
            else
            {
                auto const next_index = std::min(index, impl_->documents.size() - 1);
                impl_->active_document_id = impl_->documents[next_index].id;
            }
        }

        return true;
    }

    bool Workspace::ActivateDocument(DocumentId const id)
    {
        if (!impl_->Find(id))
        {
            return false;
        }

        impl_->active_document_id = id;
        return true;
    }

    std::optional<DocumentId> Workspace::ActiveDocument() const noexcept
    {
        return impl_->active_document_id;
    }

    bool Workspace::Contains(DocumentId const id) const noexcept
    {
        return impl_->Find(id) != nullptr;
    }

    bool Workspace::ActivateLayer(DocumentId const document_id, LayerId const layer_id)
    {
        auto* const open_document = impl_->Find(document_id);
        if (!open_document || !detail::Find(open_document->layers, layer_id))
        {
            return false;
        }
        open_document->layers.active_layer_id = layer_id;
        return true;
    }

    bool Workspace::ActivateLayer(LayerId const layer_id)
    {
        auto* const active = impl_->Active();
        return active ? ActivateLayer(active->id, layer_id) : false;
    }

    void Workspace::ExecuteCommand(
        DocumentId const id,
        std::unique_ptr<DocumentCommand> command)
    {
        auto* const open_document = impl_->Find(id);
        if (!open_document)
        {
            throw std::out_of_range("The document is not open in this workspace.");
        }
        if (!command)
        {
            throw std::invalid_argument("A document command cannot be null.");
        }

        DocumentMutation mutation{ &open_document->document, &open_document->layers };
        command->Execute(mutation);

        if (open_document->history_position < open_document->history.size())
        {
            open_document->history.erase(
                open_document->history.begin()
                    + static_cast<std::ptrdiff_t>(open_document->history_position),
                open_document->history.end());
        }

        auto const before_revision = open_document->current_revision;
        auto const after_revision = open_document->next_revision++;
        open_document->history.push_back({
            .command = std::move(command),
            .before_revision = before_revision,
            .after_revision = after_revision
        });
        open_document->history_position = open_document->history.size();
        open_document->current_revision = after_revision;
    }

    bool Workspace::Undo(DocumentId const id)
    {
        auto* const open_document = impl_->Find(id);
        if (!open_document)
        {
            throw std::out_of_range("The document is not open in this workspace.");
        }
        if (open_document->history_position == 0)
        {
            return false;
        }

        auto& entry = open_document->history[open_document->history_position - 1];
        DocumentMutation mutation{ &open_document->document, &open_document->layers };
        entry.command->Undo(mutation);
        --open_document->history_position;
        open_document->current_revision = entry.before_revision;
        return true;
    }

    bool Workspace::Redo(DocumentId const id)
    {
        auto* const open_document = impl_->Find(id);
        if (!open_document)
        {
            throw std::out_of_range("The document is not open in this workspace.");
        }
        if (open_document->history_position == open_document->history.size())
        {
            return false;
        }

        auto& entry = open_document->history[open_document->history_position];
        DocumentMutation mutation{ &open_document->document, &open_document->layers };
        entry.command->Execute(mutation);
        ++open_document->history_position;
        open_document->current_revision = entry.after_revision;
        return true;
    }

    void Workspace::MarkSaved(DocumentId const id)
    {
        auto* const open_document = impl_->Find(id);
        if (!open_document)
        {
            throw std::out_of_range("The document is not open in this workspace.");
        }

        open_document->saved_revision = open_document->current_revision;
    }

    void Workspace::ExecuteCommand(std::unique_ptr<DocumentCommand> command)
    {
        if (!impl_->active_document_id)
        {
            throw std::logic_error("The workspace has no active document.");
        }
        ExecuteCommand(*impl_->active_document_id, std::move(command));
    }

    bool Workspace::Undo()
    {
        if (!impl_->active_document_id)
        {
            return false;
        }
        return Undo(*impl_->active_document_id);
    }

    bool Workspace::Redo()
    {
        if (!impl_->active_document_id)
        {
            return false;
        }
        return Redo(*impl_->active_document_id);
    }

    void Workspace::MarkActiveDocumentSaved()
    {
        if (!impl_->active_document_id)
        {
            throw std::logic_error("The workspace has no active document.");
        }
        MarkSaved(*impl_->active_document_id);
    }

    PaintStrokeResult Workspace::ApplyPaintStroke(PaintStrokeRequest const& request)
    {
        auto* const open_document = impl_->Find(request.document_id);
        if (!open_document)
        {
            return { .status = PaintStrokeStatus::DocumentNotFound };
        }
        auto* const layer = detail::Find(open_document->layers, request.layer_id);
        if (!layer)
        {
            return { .status = PaintStrokeStatus::LayerNotFound };
        }
        if (layer->Kind() != core::LayerKind::Raster)
        {
            return { .status = PaintStrokeStatus::LayerNotRaster };
        }
        if (layer->Properties().locked)
        {
            return { .status = PaintStrokeStatus::LayerLocked };
        }
        if (request.samples.empty()
            || (request.tool != PaintTool::Pencil && request.tool != PaintTool::Airbrush))
        {
            return { .status = PaintStrokeStatus::InvalidRequest };
        }

        try
        {
            auto const document_size = open_document->document.Size();
            if (document_size.width > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())
                || document_size.height > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()))
            {
                return { .status = PaintStrokeStatus::InvalidRequest };
            }

            // Reserve before touching pixels, so allocation failure cannot leave
            // a stroke applied without its matching history entry.
            open_document->history.reserve(open_document->history.size() + 1);
            auto command = std::make_unique<PaintStrokeCommand>(
                request,
                CanvasSize{ document_size.width, document_size.height });
            DocumentMutation mutation{ &open_document->document, &open_document->layers };
            command->Execute(mutation);
            if (!command->Changed())
            {
                return { .status = PaintStrokeStatus::NoPixels };
            }

            auto const bounds = command->ChangedBounds();
            if (open_document->history_position < open_document->history.size())
            {
                open_document->history.erase(
                    open_document->history.begin()
                        + static_cast<std::ptrdiff_t>(open_document->history_position),
                    open_document->history.end());
            }
            auto const before_revision = open_document->current_revision;
            auto const after_revision = open_document->next_revision++;
            open_document->history.push_back({
                .command = std::move(command),
                .before_revision = before_revision,
                .after_revision = after_revision
            });
            open_document->history_position = open_document->history.size();
            open_document->current_revision = after_revision;
            return { .status = PaintStrokeStatus::Applied, .changed_bounds = bounds };
        }
        catch (std::invalid_argument const&)
        {
            return { .status = PaintStrokeStatus::InvalidRequest };
        }
        catch (std::length_error const&)
        {
            return { .status = PaintStrokeStatus::InvalidRequest };
        }
        catch (std::overflow_error const&)
        {
            return { .status = PaintStrokeStatus::InvalidRequest };
        }
    }

    std::optional<RasterPixelSnapshot> Workspace::SnapshotRasterLayerPixels(
        DocumentId const document_id,
        LayerId const layer_id) const
    {
        auto const* const open_document = impl_->Find(document_id);
        if (!open_document)
        {
            return std::nullopt;
        }
        auto const* const layer = detail::Find(open_document->layers, layer_id);
        if (!layer || layer->Kind() != core::LayerKind::Raster)
        {
            return std::nullopt;
        }

        auto const core_size = open_document->document.Size();
        auto const width = static_cast<std::size_t>(core_size.width);
        auto const height = static_cast<std::size_t>(core_size.height);
        if (width > std::numeric_limits<std::size_t>::max() / core::Rgba8BytesPerPixel)
        {
            throw std::length_error("Raster snapshot row stride exceeds addressable memory.");
        }
        auto const row_stride = width * core::Rgba8BytesPerPixel;
        if (height != 0 && row_stride > std::numeric_limits<std::size_t>::max() / height)
        {
            throw std::length_error("Raster snapshot exceeds addressable memory.");
        }

        RasterPixelSnapshot snapshot{
            .document_id = document_id,
            .layer_id = layer_id,
            .size = { core_size.width, core_size.height },
            .revision = open_document->current_revision,
            .row_stride = row_stride,
            .pixels_bgra_premultiplied = std::vector<std::byte>(row_stride * height)
        };

        auto const& tiles = static_cast<core::RasterLayer const*>(layer)->Tiles();
        for (auto const key : tiles.Keys())
        {
            if (key.level != 0)
            {
                continue;
            }
            auto const payload = tiles.Read(key);
            if (!payload)
            {
                continue;
            }
            auto const source = payload->Pixels();
            auto const origin_x = static_cast<std::int64_t>(key.x) * core::TileExtent;
            auto const origin_y = static_cast<std::int64_t>(key.y) * core::TileExtent;
            for (std::uint32_t local_y = 0; local_y < core::TileExtent; ++local_y)
            {
                auto const canvas_y = origin_y + local_y;
                if (canvas_y < 0 || canvas_y >= static_cast<std::int64_t>(core_size.height))
                {
                    continue;
                }
                for (std::uint32_t local_x = 0; local_x < core::TileExtent; ++local_x)
                {
                    auto const canvas_x = origin_x + local_x;
                    if (canvas_x < 0 || canvas_x >= static_cast<std::int64_t>(core_size.width))
                    {
                        continue;
                    }
                    auto const source_index =
                        (static_cast<std::size_t>(local_y) * core::TileExtent + local_x)
                        * core::Rgba8BytesPerPixel;
                    auto const destination_index = static_cast<std::size_t>(canvas_y) * row_stride
                        + static_cast<std::size_t>(canvas_x) * core::Rgba8BytesPerPixel;
                    snapshot.pixels_bgra_premultiplied[destination_index] = source[source_index + 2];
                    snapshot.pixels_bgra_premultiplied[destination_index + 1] = source[source_index + 1];
                    snapshot.pixels_bgra_premultiplied[destination_index + 2] = source[source_index];
                    snapshot.pixels_bgra_premultiplied[destination_index + 3] = source[source_index + 3];
                }
            }
        }
        return snapshot;
    }

    std::optional<CompositePixelSnapshot> Workspace::SnapshotCompositePixels(
        DocumentId const document_id) const
    {
        auto const* const open_document = impl_->Find(document_id);
        if (!open_document)
        {
            return std::nullopt;
        }

        auto const size = open_document->document.Size();
        auto pixels = detail::MakeCanvasPixelBuffer(size);
        detail::CompositeLayers(open_document->layers.tree.Roots(), size, pixels);
        return CompositePixelSnapshot{
            .document_id = document_id,
            .size = { size.width, size.height },
            .revision = open_document->current_revision,
            .row_stride = pixels.row_stride,
            .pixels_bgra_premultiplied = std::move(pixels.pixels)
        };
    }

    SelectionResult Workspace::ApplySelectionGesture(SelectionGestureRequest const& request)
    {
        auto* const open_document = impl_->Find(request.document_id);
        if (!open_document)
        {
            return { .status = SelectionStatus::DocumentNotFound };
        }

        try
        {
            auto const size = open_document->document.Size();
            if (size.width > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())
                || size.height > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()))
            {
                return { .status = SelectionStatus::InvalidRequest };
            }
            core::RectI const canvas{
                0,
                0,
                static_cast<std::int32_t>(size.width),
                static_cast<std::int32_t>(size.height)
            };

            core::SelectionMask selection;
            switch (request.kind)
            {
            case SelectionGestureKind::Rectangular:
                selection = core::RasterizeRectangularSelection(canvas, {
                    request.bounds.x,
                    request.bounds.y,
                    request.bounds.width,
                    request.bounds.height });
                break;
            case SelectionGestureKind::Elliptical:
                selection = core::RasterizeEllipticalSelection(canvas, {
                    request.bounds.x,
                    request.bounds.y,
                    request.bounds.width,
                    request.bounds.height });
                break;
            case SelectionGestureKind::Freehand:
            case SelectionGestureKind::Polygonal:
            {
                std::vector<core::PointI> points;
                points.reserve(request.points.size());
                for (auto const point : request.points)
                {
                    points.push_back({ point.x, point.y });
                }
                selection = request.kind == SelectionGestureKind::Freehand
                    ? core::RasterizeFreehandSelection(canvas, points)
                    : core::RasterizePolygonalSelection(canvas, points);
                break;
            }
            default:
                return { .status = SelectionStatus::InvalidRequest };
            }

            if (detail::SelectionMasksEqual(open_document->layers.selection, selection))
            {
                return { .status = SelectionStatus::NoChange };
            }

            open_document->history.reserve(open_document->history.size() + 1);
            auto command = std::make_unique<SelectionCommand>(std::move(selection));
            DocumentMutation mutation{ &open_document->document, &open_document->layers };
            command->Execute(mutation);

            if (open_document->history_position < open_document->history.size())
            {
                open_document->history.erase(
                    open_document->history.begin()
                        + static_cast<std::ptrdiff_t>(open_document->history_position),
                    open_document->history.end());
            }
            auto const before_revision = open_document->current_revision;
            auto const after_revision = open_document->next_revision++;
            open_document->history.push_back({
                .command = std::move(command),
                .before_revision = before_revision,
                .after_revision = after_revision
            });
            open_document->history_position = open_document->history.size();
            open_document->current_revision = after_revision;
            return { .status = SelectionStatus::Applied };
        }
        catch (std::invalid_argument const&)
        {
            return { .status = SelectionStatus::InvalidRequest };
        }
        catch (std::length_error const&)
        {
            return { .status = SelectionStatus::InvalidRequest };
        }
        catch (std::overflow_error const&)
        {
            return { .status = SelectionStatus::InvalidRequest };
        }
    }

    std::optional<SelectionBoundarySnapshot> Workspace::SnapshotSelectionBoundary(
        DocumentId const document_id) const
    {
        auto const* const open_document = impl_->Find(document_id);
        if (!open_document)
        {
            return std::nullopt;
        }

        auto const size = open_document->document.Size();
        SelectionBoundarySnapshot snapshot{
            .document_id = document_id,
            .canvas_size = { size.width, size.height },
            .revision = open_document->current_revision
        };
        auto const& selection = open_document->layers.selection;
        auto const bounds = selection.Bounds();
        for (std::int64_t y = bounds.y;
            y < static_cast<std::int64_t>(bounds.y) + bounds.height;
            ++y)
        {
            for (std::int64_t x = bounds.x;
                x < static_cast<std::int64_t>(bounds.x) + bounds.width;
                ++x)
            {
                core::PointI const pixel{
                    static_cast<std::int32_t>(x),
                    static_cast<std::int32_t>(y)
                };
                if (selection.CoverageAt(pixel) == 0)
                {
                    continue;
                }
                snapshot.has_selection = true;
                auto const left = pixel.x;
                auto const top = pixel.y;
                auto const right = static_cast<std::int32_t>(pixel.x + 1);
                auto const bottom = static_cast<std::int32_t>(pixel.y + 1);
                if (selection.CoverageAt({ pixel.x, static_cast<std::int32_t>(pixel.y - 1) }) == 0)
                {
                    snapshot.edges.push_back({ { left, top }, { right, top } });
                }
                if (selection.CoverageAt({ static_cast<std::int32_t>(pixel.x + 1), pixel.y }) == 0)
                {
                    snapshot.edges.push_back({ { right, top }, { right, bottom } });
                }
                if (selection.CoverageAt({ pixel.x, static_cast<std::int32_t>(pixel.y + 1) }) == 0)
                {
                    snapshot.edges.push_back({ { right, bottom }, { left, bottom } });
                }
                if (selection.CoverageAt({ static_cast<std::int32_t>(pixel.x - 1), pixel.y }) == 0)
                {
                    snapshot.edges.push_back({ { left, bottom }, { left, top } });
                }
            }
        }
        return snapshot;
    }

    WorkspaceSnapshot Workspace::Snapshot() const
    {
        WorkspaceSnapshot snapshot;
        snapshot.documents.reserve(impl_->documents.size());
        snapshot.active_document_id = impl_->active_document_id;

        for (auto const& open_document : impl_->documents)
        {
            CommandAvailability commands;
            commands.can_undo = open_document.history_position > 0;
            commands.can_redo = open_document.history_position < open_document.history.size();
            if (commands.can_undo)
            {
                commands.undo_label = open_document.history[open_document.history_position - 1].command->Label();
            }
            if (commands.can_redo)
            {
                commands.redo_label = open_document.history[open_document.history_position].command->Label();
            }

            auto const size = open_document.document.Size();
            DocumentSummary summary{
                .id = open_document.id,
                .title = open_document.document.Title(),
                .canvas_size = { .width = size.width, .height = size.height },
                .is_active = impl_->active_document_id == open_document.id,
                .is_dirty = open_document.current_revision != open_document.saved_revision,
                .current_revision = open_document.current_revision,
                .saved_revision = open_document.saved_revision,
                .commands = std::move(commands),
                .active_layer_id = open_document.layers.active_layer_id
            };

            auto collect_layers = [&summary](
                auto&& self,
                std::span<std::unique_ptr<core::Layer> const> const layers,
                std::optional<LayerId> const parent,
                std::size_t const depth) -> void
            {
                for (std::size_t index = 0; index < layers.size(); ++index)
                {
                    auto const& layer = *layers[index];
                    auto const& properties = layer.Properties();
                    LayerId const layer_id{ properties.id.Value() };
                    summary.layers.push_back({
                        .id = layer_id,
                        .parent_id = parent,
                        .sibling_index = index,
                        .depth = depth,
                        .kind = static_cast<LayerKind>(layer.Kind()),
                        .name = properties.name_utf8,
                        .visible = properties.visible,
                        .locked = properties.locked,
                        .alpha_locked = properties.alpha_locked,
                        .opacity = properties.opacity,
                        .blend_mode = static_cast<BlendMode>(properties.blend_mode)
                    });

                    if (layer.Kind() == core::LayerKind::Group)
                    {
                        self(
                            self,
                            static_cast<core::GroupLayer const&>(layer).Children(),
                            layer_id,
                            depth + 1);
                    }
                }
            };
            collect_layers(collect_layers, open_document.layers.tree.Roots(), std::nullopt, 0);
            snapshot.documents.push_back(std::move(summary));
        }

        auto const* const active = impl_->Active();
        if (!active)
        {
            snapshot.document_title = "No document";
            snapshot.status_message = "Ready";
            return snapshot;
        }

        snapshot.has_document = true;
        snapshot.document_title = active->document.Title();
        auto const active_size = active->document.Size();
        snapshot.canvas_size = { .width = active_size.width, .height = active_size.height };
        snapshot.status_message = std::format("{} x {} pixels", active_size.width, active_size.height);
        snapshot.active_commands = snapshot.documents[
            static_cast<std::size_t>(active - impl_->documents.data())].commands;
        snapshot.active_layer_id = active->layers.active_layer_id;
        return snapshot;
    }
}
