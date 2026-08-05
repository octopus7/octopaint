#include <octopaint/application/Workspace.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace
{
    void Require(bool const condition, std::string_view const message)
    {
        if (!condition)
        {
            throw std::runtime_error(std::string{ message });
        }
    }

    [[nodiscard]] octopaint::application::DocumentSummary const& Find(
        octopaint::application::WorkspaceSnapshot const& snapshot,
        octopaint::application::DocumentId const id)
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
}

int main()
{
    using namespace octopaint::application;

    try
    {
        Workspace workspace;
        auto const empty = workspace.Snapshot();
        Require(!empty.has_document && empty.documents.empty(), "A new workspace must be empty.");
        Require(!workspace.Undo() && !workspace.Redo(), "Empty workspace history operations must be harmless.");

        auto const first = workspace.NewDocument("First", { 800, 600 });
        auto const second = workspace.NewDocument("Second", { 1920, 1080 });
        Require(first != second && first && second, "Document IDs must be valid and unique.");
        Require(workspace.ActiveDocument() == second, "The newest document must become active.");

        Require(workspace.ActivateDocument(first), "An open document must be activatable.");
        workspace.ExecuteCommand(std::make_unique<RenameDocumentCommand>("First renamed"));

        auto after_rename = workspace.Snapshot();
        auto const& renamed = Find(after_rename, first);
        auto const& untouched = Find(after_rename, second);
        Require(renamed.title == "First renamed" && renamed.is_dirty, "A command must mutate and dirty its document.");
        Require(renamed.commands.can_undo && renamed.commands.undo_label == "Rename document", "Undo label must be exposed.");
        Require(!untouched.is_dirty && !untouched.commands.can_undo, "Histories must be isolated per document.");

        workspace.MarkSaved(first);
        Require(!Find(workspace.Snapshot(), first).is_dirty, "MarkSaved must capture the current revision.");
        Require(workspace.Undo(first), "A recorded command must be undoable.");

        auto after_undo = workspace.Snapshot();
        auto const& undone = Find(after_undo, first);
        Require(undone.title == "First" && undone.is_dirty, "Undoing past the saved revision must be dirty.");
        Require(undone.commands.can_redo && undone.commands.redo_label == "Rename document", "Redo label must be exposed.");

        Require(workspace.Redo(first), "An undone command must be redoable.");
        Require(!Find(workspace.Snapshot(), first).is_dirty, "Returning to the saved revision must be clean.");

        Require(workspace.Undo(first), "The rename must remain undoable.");
        workspace.ExecuteCommand(first, std::make_unique<RenameDocumentCommand>("Branched"));
        auto const branched = Find(workspace.Snapshot(), first);
        Require(!branched.commands.can_redo, "A new command after undo must discard the redo branch.");
        Require(branched.current_revision != branched.saved_revision, "A new branch must have a distinct revision.");

        Require(workspace.CloseDocument(first), "An open document must close.");
        Require(!workspace.Contains(first), "A closed identity must no longer resolve.");
        Require(workspace.ActiveDocument() == second, "Closing the active document must select a neighbor.");
        auto const third = workspace.NewDocument("Third", { 32, 32 });
        Require(third != first, "Closed document IDs must never be reused.");
        Require(!workspace.ActivateDocument(first) && !workspace.CloseDocument(first), "Stale IDs must be rejected.");

        auto detached = workspace.Snapshot();
        detached.documents.front().title = "Snapshot only";
        Require(Find(workspace.Snapshot(), second).title == "Second", "Snapshots must not expose mutable workspace state.");

        std::cout << "OctoPaint application tests passed.\n";
        return EXIT_SUCCESS;
    }
    catch (std::exception const& error)
    {
        std::cerr << "OctoPaint application tests failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
