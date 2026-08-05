#include <octopaint/application/Workspace.h>

#include <cmath>
#include <cstdlib>
#include <iostream>
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

    [[nodiscard]] DocumentSummary const& FindDocument(
        WorkspaceSnapshot const& snapshot,
        DocumentId const id)
    {
        for (auto const& document : snapshot.documents)
        {
            if (document.id == id)
            {
                return document;
            }
        }
        throw std::runtime_error("Document summary was not found.");
    }

    [[nodiscard]] LayerSummary const& FindLayer(
        DocumentSummary const& document,
        LayerId const id)
    {
        for (auto const& layer : document.layers)
        {
            if (layer.id == id)
            {
                return layer;
            }
        }
        throw std::runtime_error("Layer summary was not found.");
    }
}

int main()
{
    using namespace octopaint::application;

    try
    {
        Workspace workspace;
        auto const first_document = workspace.NewDocument("First", { 800, 600 });
        auto const second_document = workspace.NewDocument("Second", { 640, 480 });
        auto initial = workspace.Snapshot();
        auto const first_default = FindDocument(initial, first_document).layers.front().id;
        auto const second_default = FindDocument(initial, second_document).layers.front().id;

        Require(first_default != second_default, "Default layer IDs must be unique across documents.");
        Require(FindDocument(initial, first_document).layers.size() == 1, "Each new document must have one default layer.");
        Require(FindDocument(initial, first_document).active_layer_id == first_default, "The default layer must be active.");
        Require(FindLayer(FindDocument(initial, first_document), first_default).kind == LayerKind::Raster,
            "The default layer must be raster.");

        auto add_group = std::make_unique<AddGroupLayerCommand>("Paint group");
        auto const group = add_group->CreatedLayerId();
        workspace.ExecuteCommand(first_document, std::move(add_group));

        auto add_child = std::make_unique<AddRasterLayerCommand>("Ink", group);
        auto const child = add_child->CreatedLayerId();
        workspace.ExecuteCommand(first_document, std::move(add_child));

        auto nested = workspace.Snapshot();
        auto const& nested_first = FindDocument(nested, first_document);
        Require(nested_first.layers.size() == 3, "Nested layer creation must appear in the snapshot.");
        Require(nested_first.layers[0].id == first_default
            && nested_first.layers[1].id == group
            && nested_first.layers[2].id == child,
            "Layer snapshots must use stable pre-order traversal.");
        Require(nested_first.layers[2].parent_id == group
            && nested_first.layers[2].depth == 1
            && nested_first.layers[2].sibling_index == 0,
            "Nested layer location metadata must be complete.");
        Require(nested_first.active_layer_id == child, "A newly added layer must become active.");
        Require(FindDocument(nested, second_document).layers.size() == 1,
            "Layer mutation must remain isolated to its document.");

        Require(workspace.Undo(first_document), "Adding a raster layer must be undoable.");
        auto child_add_undone = workspace.Snapshot();
        Require(FindDocument(child_add_undone, first_document).layers.size() == 2
            && FindDocument(child_add_undone, first_document).active_layer_id == group,
            "Undo raster add must remove it and restore the previous active layer.");
        Require(workspace.Undo(first_document), "Adding a group layer must be undoable.");
        auto group_add_undone = workspace.Snapshot();
        Require(FindDocument(group_add_undone, first_document).layers.size() == 1
            && FindDocument(group_add_undone, first_document).active_layer_id == first_default,
            "Undo group add must remove it and restore the default active layer.");
        Require(workspace.Redo(first_document), "Adding a group layer must be redoable.");
        Require(workspace.Redo(first_document), "Adding a raster layer must be redoable.");
        auto adds_redone = workspace.Snapshot();
        Require(FindDocument(adds_redone, first_document).layers.size() == 3
            && FindDocument(adds_redone, first_document).active_layer_id == child,
            "Redo add commands must restore the nested layers and active layer.");

        workspace.ExecuteCommand(first_document, std::make_unique<MoveLayerCommand>(first_default, group, 1));
        auto moved = workspace.Snapshot();
        auto const& moved_first = FindDocument(moved, first_document);
        Require(moved_first.layers[0].id == group
            && moved_first.layers[1].id == child
            && moved_first.layers[2].id == first_default,
            "Move must preserve the requested child order.");
        Require(moved_first.layers[2].parent_id == group && moved_first.layers[2].sibling_index == 1,
            "Move snapshot metadata must describe the new location.");

        Require(workspace.Undo(first_document), "Move must be undoable.");
        auto move_undone = workspace.Snapshot();
        Require(FindLayer(FindDocument(move_undone, first_document), first_default).parent_id == std::nullopt,
            "Undo move must restore the original parent.");
        Require(workspace.Redo(first_document), "Move must be redoable.");
        Require(FindLayer(FindDocument(workspace.Snapshot(), first_document), first_default).parent_id == group,
            "Redo move must restore the destination parent.");

        workspace.ExecuteCommand(first_document, std::make_unique<RenameLayerCommand>(child, "Line art"));
        workspace.ExecuteCommand(first_document, std::make_unique<SetLayerOpacityCommand>(child, 0.4F));
        workspace.ExecuteCommand(first_document, std::make_unique<SetLayerVisibilityCommand>(child, false));
        workspace.ExecuteCommand(first_document, std::make_unique<SetLayerAlphaLockedCommand>(child, true));
        workspace.ExecuteCommand(first_document, std::make_unique<SetLayerBlendModeCommand>(child, BlendMode::Multiply));
        auto styled = workspace.Snapshot();
        auto const& styled_layer = FindLayer(FindDocument(styled, first_document), child);
        Require(styled_layer.name == "Line art", "Rename command must update the layer.");
        Require(std::abs(styled_layer.opacity - 0.4F) < 0.0001F, "Opacity command must update the layer.");
        Require(!styled_layer.visible, "Visibility command must update the layer.");
        Require(styled_layer.alpha_locked, "Alpha lock command must update the layer.");
        Require(styled_layer.blend_mode == BlendMode::Multiply, "Blend command must update the layer.");

        workspace.MarkSaved(first_document);
        Require(!FindDocument(workspace.Snapshot(), first_document).is_dirty,
            "Saving must capture the revision after layer commands.");
        Require(workspace.Undo(first_document), "Blend mode must be undoable.");
        Require(FindLayer(FindDocument(workspace.Snapshot(), first_document), child).blend_mode == BlendMode::Normal,
            "Undo blend mode must restore its previous value.");
        Require(FindDocument(workspace.Snapshot(), first_document).is_dirty,
            "Undoing away from the saved layer revision must be dirty.");
        Require(workspace.Undo(first_document), "Alpha lock must be undoable.");
        Require(!FindLayer(FindDocument(workspace.Snapshot(), first_document), child).alpha_locked,
            "Undo alpha lock must restore its previous value.");
        Require(workspace.Undo(first_document), "Visibility must be undoable.");
        Require(FindLayer(FindDocument(workspace.Snapshot(), first_document), child).visible,
            "Undo visibility must restore its previous value.");
        Require(workspace.Undo(first_document), "Opacity must be undoable.");
        Require(std::abs(FindLayer(FindDocument(workspace.Snapshot(), first_document), child).opacity - 1.0F) < 0.0001F,
            "Undo opacity must restore its previous value.");
        Require(workspace.Undo(first_document), "Rename must be undoable.");
        Require(FindLayer(FindDocument(workspace.Snapshot(), first_document), child).name == "Ink",
            "Undo rename must restore its previous value.");
        Require(workspace.Redo(first_document), "Rename must be redoable.");
        Require(workspace.Redo(first_document), "Opacity must be redoable.");
        Require(workspace.Redo(first_document), "Visibility must be redoable.");
        Require(workspace.Redo(first_document), "Alpha lock must be redoable.");
        auto const alpha_redone = FindLayer(FindDocument(workspace.Snapshot(), first_document), child);
        Require(alpha_redone.name == "Line art"
            && std::abs(alpha_redone.opacity - 0.4F) < 0.0001F
            && !alpha_redone.visible
            && alpha_redone.alpha_locked,
            "Redo must restore all requested layer properties exactly.");
        Require(workspace.Redo(first_document), "Blend mode must be redoable.");
        Require(!FindDocument(workspace.Snapshot(), first_document).is_dirty,
            "Redoing to the saved layer revision must be clean.");

        workspace.ExecuteCommand(first_document, std::make_unique<RemoveLayerCommand>(group));
        auto removed = workspace.Snapshot();
        Require(FindDocument(removed, first_document).layers.empty(), "Removing a group must remove its subtree.");
        Require(!FindDocument(removed, first_document).active_layer_id, "Removing the active subtree must clear active layer when empty.");
        Require(workspace.Undo(first_document), "Removing a subtree must be undoable.");
        auto removal_undone = workspace.Snapshot();
        auto const& restored = FindDocument(removal_undone, first_document);
        Require(restored.layers.size() == 3, "Undo remove must restore the complete subtree.");
        Require(restored.active_layer_id == child, "Undo remove must restore the exact active layer.");
        Require(FindLayer(restored, first_default).parent_id == group,
            "Undo remove must preserve nested ordering and parentage.");

        Require(workspace.ActivateLayer(first_document, first_default), "An existing layer must be activatable.");
        Require(!workspace.ActivateLayer(second_document, first_default), "A layer cannot be activated in another document.");
        Require(FindDocument(workspace.Snapshot(), second_document).layers.front().id == second_default,
            "The second document must remain unchanged after all first-document commands.");

        Require(workspace.CloseDocument(first_document), "The first document must close.");
        auto const third_document = workspace.NewDocument("Third", { 32, 32 });
        auto const third_default = FindDocument(workspace.Snapshot(), third_document).layers.front().id;
        Require(third_default != first_default && third_default != group && third_default != child,
            "Layer IDs from closed documents must never be reused.");

        std::cout << "OctoPaint application layer tests passed.\n";
        return EXIT_SUCCESS;
    }
    catch (std::exception const& error)
    {
        std::cerr << "OctoPaint application layer tests failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
