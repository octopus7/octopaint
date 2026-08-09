#include <octopaint/application/Workspace.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

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

    [[nodiscard]] LayerId ActiveLayer(Workspace const& workspace, DocumentId const document)
    {
        auto const snapshot = workspace.Snapshot();
        for (auto const& candidate : snapshot.documents)
        {
            if (candidate.id == document)
            {
                Require(candidate.active_layer_id.has_value(), "The test document must have an active layer.");
                return *candidate.active_layer_id;
            }
        }
        throw std::runtime_error("The test document was not found.");
    }

    template<typename Snapshot>
    [[nodiscard]] std::uint8_t ByteAt(
        Snapshot const& snapshot,
        std::uint32_t const x,
        std::uint32_t const y,
        std::size_t const channel)
    {
        return std::to_integer<std::uint8_t>(
            snapshot.pixels_bgra_premultiplied[
                static_cast<std::size_t>(y) * snapshot.row_stride
                + static_cast<std::size_t>(x) * 4 + channel]);
    }

    template<typename Snapshot>
    void RequirePixel(
        Snapshot const& snapshot,
        std::uint32_t const x,
        std::uint32_t const y,
        PaintColorRgba8 const expected,
        std::uint8_t const tolerance,
        std::string_view const message)
    {
        auto const near = [tolerance](std::uint8_t const actual, std::uint8_t const wanted)
        {
            auto const difference = actual > wanted ? actual - wanted : wanted - actual;
            return difference <= tolerance;
        };

        Require(near(ByteAt(snapshot, x, y, 0), expected.blue)
            && near(ByteAt(snapshot, x, y, 1), expected.green)
            && near(ByteAt(snapshot, x, y, 2), expected.red)
            && near(ByteAt(snapshot, x, y, 3), expected.alpha), message);
    }

    [[nodiscard]] LayerId AddRaster(
        Workspace& workspace,
        DocumentId const document,
        std::string name,
        std::optional<LayerId> const parent = std::nullopt)
    {
        auto command = std::make_unique<AddRasterLayerCommand>(std::move(name), parent);
        auto const layer = command->CreatedLayerId();
        workspace.ExecuteCommand(document, std::move(command));
        return layer;
    }

    [[nodiscard]] LayerId AddGroup(
        Workspace& workspace,
        DocumentId const document,
        std::string name)
    {
        auto command = std::make_unique<AddGroupLayerCommand>(std::move(name));
        auto const layer = command->CreatedLayerId();
        workspace.ExecuteCommand(document, std::move(command));
        return layer;
    }

    void Paint(
        Workspace& workspace,
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
        Require(workspace.ApplyPaintStroke(request).status == PaintStrokeStatus::Applied,
            "A test pencil stroke must be applied.");
    }

    void TestNormalVisibilityAndOpacity()
    {
        Workspace workspace;
        auto const document = workspace.NewDocument("Normal composite", { 3, 1 });
        auto const background = ActiveLayer(workspace, document);
        Paint(workspace, document, background, { 0, 255, 0, 255 }, { { 1.0, 0.0 } });

        auto const foreground = AddRaster(workspace, document, "Foreground");
        Paint(workspace, document, foreground, { 255, 0, 0, 255 }, { { 0.0, 0.0 } });

        auto composite = workspace.SnapshotCompositePixels(document);
        Require(composite && composite->document_id == document
                && composite->size == CanvasSize{ 3, 1 }
                && composite->row_stride == 12
                && composite->pixels_bgra_premultiplied.size() == 12,
            "A composite snapshot must be detached, tightly packed, and canvas sized.");
        RequirePixel(*composite, 0, 0, { 255, 0, 0, 255 }, 0,
            "The upper raster layer must contribute its red pixel.");
        RequirePixel(*composite, 1, 0, { 0, 255, 0, 255 }, 0,
            "A transparent upper pixel must reveal the lower raster layer.");
        RequirePixel(*composite, 2, 0, { 255, 255, 255, 255 }, 0,
            "Unpainted pixels must retain the opaque white document background.");

        workspace.ExecuteCommand(document,
            std::make_unique<SetLayerOpacityCommand>(foreground, 0.5F));
        composite = workspace.SnapshotCompositePixels(document);
        Require(composite.has_value(), "An opacity change must retain a composite snapshot.");
        RequirePixel(*composite, 0, 0, { 255, 128, 128, 255 }, 1,
            "Normal blending must apply layer opacity before source-over compositing.");

        workspace.ExecuteCommand(document,
            std::make_unique<SetLayerVisibilityCommand>(foreground, false));
        composite = workspace.SnapshotCompositePixels(document);
        Require(composite.has_value(), "A visibility change must retain a composite snapshot.");
        RequirePixel(*composite, 0, 0, { 255, 255, 255, 255 }, 0,
            "An invisible raster layer must not contribute to the composite.");
        RequirePixel(*composite, 1, 0, { 0, 255, 0, 255 }, 0,
            "Hiding one layer must not suppress other raster layers.");
    }

    void TestMultiplyAndScreen()
    {
        Workspace workspace;
        auto const document = workspace.NewDocument("Blend modes", { 2, 1 });
        auto const background = ActiveLayer(workspace, document);
        Paint(workspace, document, background, { 128, 128, 128, 255 }, {
            { 0.0, 0.0 }, { 1.0, 0.0 }
        });
        auto const foreground = AddRaster(workspace, document, "Blend layer");
        Paint(workspace, document, foreground, { 128, 128, 128, 255 }, {
            { 0.0, 0.0 }, { 1.0, 0.0 }
        });

        workspace.ExecuteCommand(document,
            std::make_unique<SetLayerBlendModeCommand>(foreground, BlendMode::Multiply));
        auto composite = workspace.SnapshotCompositePixels(document);
        Require(composite.has_value(), "Multiply mode must produce a composite snapshot.");
        RequirePixel(*composite, 0, 0, { 64, 64, 64, 255 }, 1,
            "Opaque mid-gray multiplied by mid-gray must produce quarter intensity.");

        workspace.ExecuteCommand(document,
            std::make_unique<SetLayerBlendModeCommand>(foreground, BlendMode::Screen));
        composite = workspace.SnapshotCompositePixels(document);
        Require(composite.has_value(), "Screen mode must produce a composite snapshot.");
        RequirePixel(*composite, 1, 0, { 192, 192, 192, 255 }, 1,
            "Opaque mid-gray screened with mid-gray must produce three-quarter intensity.");
    }

    void TestGroupComposition()
    {
        Workspace workspace;
        auto const document = workspace.NewDocument("Group composite", { 2, 1 });
        auto const group = AddGroup(workspace, document, "Paint group");
        auto const child = AddRaster(workspace, document, "Child", group);
        Paint(workspace, document, child, { 0, 0, 255, 255 }, { { 0.0, 0.0 } });

        Require(workspace.ActivateLayer(document, group), "A group layer must be activatable.");
        auto composite = workspace.SnapshotCompositePixels(document);
        Require(composite.has_value(), "An active group must still expose the complete composite.");
        RequirePixel(*composite, 0, 0, { 0, 0, 255, 255 }, 0,
            "A raster child must contribute through its parent group.");

        workspace.ExecuteCommand(document,
            std::make_unique<SetLayerOpacityCommand>(group, 0.5F));
        composite = workspace.SnapshotCompositePixels(document);
        Require(composite.has_value(), "Group opacity must retain a composite snapshot.");
        RequirePixel(*composite, 0, 0, { 128, 128, 255, 255 }, 1,
            "Group opacity must attenuate the flattened child result.");

        workspace.ExecuteCommand(document,
            std::make_unique<SetLayerVisibilityCommand>(group, false));
        composite = workspace.SnapshotCompositePixels(document);
        Require(composite.has_value(), "Group visibility must retain a composite snapshot.");
        RequirePixel(*composite, 0, 0, { 255, 255, 255, 255 }, 0,
            "An invisible group must suppress all of its descendants.");
    }

    void TestSelectionConstrainsPaintingAndUndo()
    {
        Workspace workspace;
        auto const document = workspace.NewDocument("Selected paint", { 5, 1 });
        auto const layer = ActiveLayer(workspace, document);

        Paint(workspace, document, layer, { 255, 0, 0, 255 }, { { 4.0, 0.0 } });
        Require(workspace.ApplySelectionGesture({
            .document_id = document,
            .kind = SelectionGestureKind::Rectangular,
            .bounds = { 1, 0, 2, 1 }
        }).status == SelectionStatus::Applied,
            "The rectangle used to constrain paint must be applied.");

        PaintStrokeRequest constrained{
            .document_id = document,
            .layer_id = layer,
            .tool = PaintTool::Pencil,
            .color = { 0, 0, 0, 255 },
            .samples = { { 0.0, 0.0 }, { 4.0, 0.0 } }
        };
        constrained.brush.opacity = 1.0F;
        constrained.brush.pressure_affects_opacity = false;
        auto const result = workspace.ApplyPaintStroke(constrained);
        Require(result.status == PaintStrokeStatus::Applied
                && result.changed_bounds == PixelBounds{ 1, 0, 2, 1 },
            "A selection must clip both changed pixels and the reported dirty bounds.");

        auto pixels = workspace.SnapshotRasterLayerPixels(document, layer);
        Require(pixels.has_value(), "The selected paint target must remain snapshotable.");
        RequirePixel(*pixels, 0, 0, { 255, 255, 255, 255 }, 0,
            "A stroke pixel outside the selection must remain untouched.");
        RequirePixel(*pixels, 1, 0, { 0, 0, 0, 255 }, 0,
            "A stroke pixel inside the selection must be painted.");
        RequirePixel(*pixels, 2, 0, { 0, 0, 0, 255 }, 0,
            "Every selected pixel crossed by the stroke must be painted.");
        RequirePixel(*pixels, 3, 0, { 255, 255, 255, 255 }, 0,
            "Painting must stop at the selection boundary.");
        RequirePixel(*pixels, 4, 0, { 255, 0, 0, 255 }, 0,
            "Pre-existing unselected pixels must survive selected painting.");

        Require(workspace.Undo(document), "Selected painting must be undoable as one stroke.");
        pixels = workspace.SnapshotRasterLayerPixels(document, layer);
        Require(pixels.has_value(), "The raster layer must remain snapshotable after undo.");
        RequirePixel(*pixels, 1, 0, { 255, 255, 255, 255 }, 0,
            "Undo must restore the first pixel changed inside the selection.");
        RequirePixel(*pixels, 2, 0, { 255, 255, 255, 255 }, 0,
            "Undo must restore the second pixel changed inside the selection.");
        RequirePixel(*pixels, 4, 0, { 255, 0, 0, 255 }, 0,
            "Undo must not disturb pre-existing pixels outside the changed set.");
    }

    void TestLayerLockBlocksPaintingUntilUndone()
    {
        Workspace workspace;
        auto const document = workspace.NewDocument("Locked paint", { 2, 1 });
        auto const layer = ActiveLayer(workspace, document);
        workspace.ExecuteCommand(document,
            std::make_unique<SetLayerLockedCommand>(layer, true));

        PaintStrokeRequest request{
            .document_id = document,
            .layer_id = layer,
            .tool = PaintTool::Pencil,
            .color = { 0, 0, 0, 255 },
            .samples = { { 0.0, 0.0 } }
        };
        request.brush.opacity = 1.0F;
        request.brush.pressure_affects_opacity = false;
        Require(workspace.ApplyPaintStroke(request).status == PaintStrokeStatus::LayerLocked,
            "A locked raster layer must reject painting without adding history.");

        Require(workspace.Undo(document), "The layer-lock command must be undoable.");
        Require(workspace.ApplyPaintStroke(request).status == PaintStrokeStatus::Applied,
            "Undoing the layer lock must restore editability.");
        auto const pixels = workspace.SnapshotRasterLayerPixels(document, layer);
        Require(pixels.has_value(), "An unlocked raster layer must remain snapshotable.");
        RequirePixel(*pixels, 0, 0, { 0, 0, 0, 255 }, 0,
            "Painting after undoing the layer lock must change the requested pixel.");
    }
}

int main()
{
    try
    {
        TestNormalVisibilityAndOpacity();
        TestMultiplyAndScreen();
        TestGroupComposition();
        TestSelectionConstrainsPaintingAndUndo();
        TestLayerLockBlocksPaintingUntilUndone();
        std::cout << "OctoPaint application composite tests passed.\n";
        return EXIT_SUCCESS;
    }
    catch (std::exception const& error)
    {
        std::cerr << "OctoPaint application composite tests failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
