#include <octopaint/application/Workspace.h>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace
{
    using namespace octopaint::application;

    void Require(bool const condition, std::string_view const message)
    {
        if (!condition)
        {
            throw std::runtime_error(std::string{ message });
        }
    }

    [[nodiscard]] LayerId ActiveLayer(Workspace const& workspace)
    {
        auto const snapshot = workspace.Snapshot();
        Require(snapshot.active_layer_id.has_value(), "The test document must have an active layer.");
        return *snapshot.active_layer_id;
    }

    [[nodiscard]] std::uint8_t ByteAt(
        RasterPixelSnapshot const& snapshot,
        std::uint32_t const x,
        std::uint32_t const y,
        std::size_t const channel)
    {
        return std::to_integer<std::uint8_t>(
            snapshot.pixels_bgra_premultiplied[
                static_cast<std::size_t>(y) * snapshot.row_stride
                + static_cast<std::size_t>(x) * 4 + channel]);
    }

    [[nodiscard]] PaintStrokeRequest Pencil(
        DocumentId const document,
        LayerId const layer,
        PaintColorRgba8 const color,
        std::initializer_list<PaintPointerSample> const samples)
    {
        PaintStrokeRequest request{
            .document_id = document,
            .layer_id = layer,
            .tool = PaintTool::Pencil,
            .color = color,
            .samples = samples
        };
        request.brush.opacity = 1.0F;
        request.brush.pressure_affects_opacity = false;
        return request;
    }
}

int main()
{
    using namespace octopaint::application;

    try
    {
        Workspace workspace;
        auto const document = workspace.NewDocument("Paint", { 8, 8 });
        auto const layer = ActiveLayer(workspace);

        auto initial = workspace.SnapshotRasterLayerPixels(document, layer);
        Require(initial.has_value(), "A raster layer must produce a render snapshot.");
        Require(initial->size == CanvasSize{ 8, 8 } && initial->row_stride == 32,
            "The render snapshot must be tightly packed and canvas sized.");
        Require(initial->revision == 0 && initial->pixels_bgra_premultiplied.size() == 256,
            "The initial snapshot must be transparent and revision zero.");
        Require(ByteAt(*initial, 1, 1, 3) == 0, "A new raster layer must be transparent.");

        auto const red_line = Pencil(document, layer, { 255, 0, 0, 255 }, {
            { 1.0, 1.0, 1.0F, 0.0 },
            { 3.0, 1.0, 1.0F, 0.01 }
        });
        auto const painted = workspace.ApplyPaintStroke(red_line);
        Require(painted.status == PaintStrokeStatus::Applied,
            "A valid pencil stroke must be applied.");
        Require(painted.changed_bounds == PixelBounds{ 1, 1, 3, 1 },
            "The stroke must report its clipped dirty bounds.");

        auto pixels = workspace.SnapshotRasterLayerPixels(document, layer);
        Require(pixels && pixels->revision == 1, "Painting must advance the document revision.");
        for (std::uint32_t x = 1; x <= 3; ++x)
        {
            Require(ByteAt(*pixels, x, 1, 0) == 0
                && ByteAt(*pixels, x, 1, 1) == 0
                && ByteAt(*pixels, x, 1, 2) == 255
                && ByteAt(*pixels, x, 1, 3) == 255,
                "The renderer snapshot must expose premultiplied BGRA pixels.");
        }
        Require(workspace.Snapshot().active_commands.undo_label == "Pencil stroke",
            "One complete stroke must create one labeled history entry.");

        Require(workspace.Undo(document), "A paint stroke must be undoable atomically.");
        auto undone = workspace.SnapshotRasterLayerPixels(document, layer);
        Require(undone && undone->revision == 0 && ByteAt(*undone, 2, 1, 3) == 0,
            "Undo must restore the exact pre-stroke pixels and revision.");
        Require(workspace.Redo(document), "A paint stroke must be redoable atomically.");
        auto redone = workspace.SnapshotRasterLayerPixels(document, layer);
        Require(redone && redone->revision == 1 && ByteAt(*redone, 2, 1, 2) == 255,
            "Redo must restore the exact post-stroke pixels and revision.");

        // Detached snapshots must not expose mutable workspace storage.
        redone->pixels_bgra_premultiplied[0] = std::byte{ 0x7f };
        auto detached_check = workspace.SnapshotRasterLayerPixels(document, layer);
        Require(detached_check && ByteAt(*detached_check, 0, 0, 0) == 0,
            "Changing a returned snapshot must not mutate the document.");

        Workspace alpha_workspace;
        auto const alpha_document = alpha_workspace.NewDocument("Alpha", { 4, 4 });
        auto const alpha_layer = ActiveLayer(alpha_workspace);
        alpha_workspace.ExecuteCommand(alpha_document,
            std::make_unique<SetLayerAlphaLockedCommand>(alpha_layer, true));
        auto const before_empty_locked = alpha_workspace.Snapshot();
        auto const empty_locked = alpha_workspace.ApplyPaintStroke(Pencil(
            alpha_document, alpha_layer, { 0, 0, 255, 255 }, { { 1.0, 1.0, 1.0F, 0.0 } }));
        auto const after_empty_locked = alpha_workspace.Snapshot();
        Require(empty_locked.status == PaintStrokeStatus::NoPixels,
            "Alpha lock must reject paint over fully transparent pixels.");
        Require(after_empty_locked.documents.front().current_revision
                == before_empty_locked.documents.front().current_revision
            && after_empty_locked.active_commands.undo_label == "Set layer alpha lock",
            "A no-op stroke must leave pixels and history unchanged.");

        Workspace failure_workspace;
        auto const failure_document = failure_workspace.NewDocument("Failure", { 4, 4 });
        auto const failure_raster = ActiveLayer(failure_workspace);
        auto add_group = std::make_unique<AddGroupLayerCommand>("Group");
        auto const group = add_group->CreatedLayerId();
        failure_workspace.ExecuteCommand(failure_document, std::move(add_group));
        auto invalid_target = Pencil(
            failure_document, group, { 255, 255, 255, 255 }, { { 1.0, 1.0, 1.0F, 0.0 } });
        Require(failure_workspace.ApplyPaintStroke(invalid_target).status == PaintStrokeStatus::LayerNotRaster,
            "Group layers must be rejected as paint targets.");
        auto invalid_sample = Pencil(
            failure_document, failure_raster, { 255, 255, 255, 255 }, {
                { std::numeric_limits<double>::quiet_NaN(), 1.0, 1.0F, 0.0 }
            });
        Require(failure_workspace.ApplyPaintStroke(invalid_sample).status == PaintStrokeStatus::InvalidRequest,
            "Invalid samples must be rejected without mutation.");
        Require(failure_workspace.Snapshot().active_commands.undo_label == "Add group layer",
            "Rejected strokes must not add history entries.");

        Workspace airbrush_workspace;
        auto const airbrush_document = airbrush_workspace.NewDocument("Airbrush", { 16, 16 });
        auto const airbrush_layer = ActiveLayer(airbrush_workspace);
        PaintStrokeRequest airbrush{
            .document_id = airbrush_document,
            .layer_id = airbrush_layer,
            .tool = PaintTool::Airbrush,
            .color = { 0, 255, 0, 255 },
            .samples = { { 8.0, 8.0, 1.0F, 1.0 / 60.0 } }
        };
        airbrush.brush.radius = 2.0F;
        airbrush.brush.flow_per_second = 1.0F;
        airbrush.brush.opacity = 1.0F;
        airbrush.brush.pressure_affects_opacity = false;
        Require(airbrush_workspace.ApplyPaintStroke(airbrush).status == PaintStrokeStatus::Applied,
            "A valid airbrush stroke must use the Core dab engine.");
        auto airbrush_pixels = airbrush_workspace.SnapshotRasterLayerPixels(airbrush_document, airbrush_layer);
        Require(airbrush_pixels && ByteAt(*airbrush_pixels, 8, 8, 1) > 0,
            "An airbrush stroke must change its center pixel.");

        std::cout << "OctoPaint application paint tests passed.\n";
        return EXIT_SUCCESS;
    }
    catch (std::exception const& error)
    {
        std::cerr << "OctoPaint application paint tests failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
