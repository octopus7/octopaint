#include <octopaint/application/Workspace.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
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

    [[nodiscard]] bool ContainsEdge(
        SelectionBoundarySnapshot const& snapshot,
        SelectionBoundaryEdge const edge)
    {
        return std::ranges::find(snapshot.edges, edge) != snapshot.edges.end();
    }
}

int main()
{
    using namespace octopaint::application;

    try
    {
        Workspace workspace;
        auto const document = workspace.NewDocument("Selection", { 8, 6 });
        auto initial = workspace.SnapshotSelectionBoundary(document);
        Require(initial && !initial->has_selection && initial->edges.empty() && initial->revision == 0,
            "A new document must expose an empty detached selection boundary.");

        SelectionGestureRequest const rectangle{
            .document_id = document,
            .kind = SelectionGestureKind::Rectangular,
            .bounds = { 1, 1, 2, 2 }
        };
        Require(workspace.ApplySelectionGesture(rectangle).status == SelectionStatus::Applied,
            "A rectangular gesture must apply through one application command.");
        auto rectangular = workspace.SnapshotSelectionBoundary(document);
        Require(rectangular && rectangular->has_selection && rectangular->revision == 1,
            "An applied selection must advance the document revision.");
        Require(rectangular->edges.size() == 8,
            "A two-by-two selection must expose only its eight transition edges, not internal edges.");
        Require(ContainsEdge(*rectangular, { { 1, 1 }, { 2, 1 } })
            && ContainsEdge(*rectangular, { { 3, 1 }, { 3, 2 } })
            && ContainsEdge(*rectangular, { { 2, 3 }, { 1, 3 } })
            && ContainsEdge(*rectangular, { { 1, 2 }, { 1, 1 } }),
            "Boundary edge coordinates must lie exactly on selected/unselected pixel transitions.");
        Require(workspace.Snapshot().active_commands.undo_label == "Set selection",
            "A complete gesture must create one labeled history entry.");

        Require(workspace.Undo(document), "Selection replacement must be undoable atomically.");
        auto undone = workspace.SnapshotSelectionBoundary(document);
        Require(undone && !undone->has_selection && undone->edges.empty() && undone->revision == 0,
            "Undo must restore the previous empty selection.");
        Require(workspace.Redo(document), "Selection replacement must be redoable atomically.");
        auto redone = workspace.SnapshotSelectionBoundary(document);
        Require(redone && redone->edges == rectangular->edges && redone->revision == 1,
            "Redo must restore the exact boundary edge snapshot.");
        Require(workspace.ApplySelectionGesture(rectangle).status == SelectionStatus::NoChange,
            "Reapplying the same mask must not create redundant history.");

        SelectionGestureRequest const clipped_to_document_edge{
            .document_id = document,
            .kind = SelectionGestureKind::Rectangular,
            .bounds = { -2, -2, 4, 4 }
        };
        Require(workspace.ApplySelectionGesture(clipped_to_document_edge).status == SelectionStatus::Applied,
            "Selection bounds must clip to the document.");
        auto clipped = workspace.SnapshotSelectionBoundary(document);
        Require(clipped
            && ContainsEdge(*clipped, { { 0, 0 }, { 1, 0 } })
            && ContainsEdge(*clipped, { { 0, 1 }, { 0, 0 } }),
            "Marching-ants edges must include transitions at the top and left document boundary.");

        SelectionGestureRequest const full_canvas{
            .document_id = document,
            .kind = SelectionGestureKind::Rectangular,
            .bounds = { 0, 0, 8, 6 }
        };
        Require(workspace.ApplySelectionGesture(full_canvas).status == SelectionStatus::Applied,
            "A full-canvas selection must be supported.");
        auto full_boundary = workspace.SnapshotSelectionBoundary(document);
        Require(full_boundary && full_boundary->edges.size() == 28
            && ContainsEdge(*full_boundary, { { 0, 0 }, { 1, 0 } })
            && ContainsEdge(*full_boundary, { { 8, 0 }, { 8, 1 } })
            && ContainsEdge(*full_boundary, { { 8, 6 }, { 7, 6 } })
            && ContainsEdge(*full_boundary, { { 0, 1 }, { 0, 0 } }),
            "All four document sides must be emitted when selected pixels touch the canvas boundary.");

        SelectionGestureRequest const ellipse{
            .document_id = document,
            .kind = SelectionGestureKind::Elliptical,
            .bounds = { 2, 1, 5, 5 }
        };
        Require(workspace.ApplySelectionGesture(ellipse).status == SelectionStatus::Applied,
            "Elliptical selection must reuse the Core selection rasterizer.");
        auto elliptical = workspace.SnapshotSelectionBoundary(document);
        Require(elliptical && elliptical->has_selection && !elliptical->edges.empty(),
            "Elliptical masks must expose a non-empty transition boundary.");

        SelectionGestureRequest const polygon{
            .document_id = document,
            .kind = SelectionGestureKind::Polygonal,
            .points = { { 1, 1 }, { 6, 1 }, { 3, 4 } }
        };
        Require(workspace.ApplySelectionGesture(polygon).status == SelectionStatus::Applied,
            "Polygonal selection must reuse the Core closed-path rasterizer.");
        auto polygonal = workspace.SnapshotSelectionBoundary(document);
        Require(polygonal && polygonal->has_selection && !polygonal->edges.empty(),
            "Polygonal masks must expose transition edges.");

        SelectionGestureRequest const freehand{
            .document_id = document,
            .kind = SelectionGestureKind::Freehand,
            .points = { { 1, 1 }, { 5, 1 }, { 5, 4 }, { 1, 4 }, { 1, 1 } }
        };
        Require(workspace.ApplySelectionGesture(freehand).status == SelectionStatus::Applied,
            "Freehand selection must reuse the Core closed-path rasterizer.");
        auto freehand_boundary = workspace.SnapshotSelectionBoundary(document);
        Require(freehand_boundary && freehand_boundary->has_selection && !freehand_boundary->edges.empty(),
            "Freehand masks must expose transition edges.");

        auto const before_invalid = workspace.Snapshot();
        auto invalid = polygon;
        invalid.points.resize(2);
        Require(workspace.ApplySelectionGesture(invalid).status == SelectionStatus::InvalidRequest,
            "Invalid paths must be rejected by the application boundary.");
        auto const after_invalid = workspace.Snapshot();
        Require(after_invalid.active_commands.undo_label == before_invalid.active_commands.undo_label
            && after_invalid.documents.front().current_revision
                == before_invalid.documents.front().current_revision,
            "Rejected selection gestures must leave history and revision unchanged.");

        auto const other_document = workspace.NewDocument("Other", { 4, 4 });
        auto other = workspace.SnapshotSelectionBoundary(other_document);
        Require(other && !other->has_selection,
            "Selection state must remain isolated per document.");
        Require(workspace.CloseDocument(other_document), "The second document must close.");
        Require(workspace.ApplySelectionGesture({
            .document_id = other_document,
            .kind = SelectionGestureKind::Rectangular,
            .bounds = { 0, 0, 1, 1 }
        }).status == SelectionStatus::DocumentNotFound,
            "Closed document IDs must be rejected without mutation.");
        Require(!workspace.SnapshotSelectionBoundary(other_document),
            "Closed documents must not produce selection snapshots.");

        std::cout << "OctoPaint application selection tests passed.\n";
        return EXIT_SUCCESS;
    }
    catch (std::exception const& error)
    {
        std::cerr << "OctoPaint application selection tests failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
