#include <octopaint/application/Workspace.h>

#include <octopaint/core/Document.h>

#include <algorithm>
#include <atomic>
#include <format>
#include <limits>
#include <stdexcept>
#include <utility>

namespace
{
    std::atomic_uint64_t next_document_id{ 1 };

    [[nodiscard]] std::uint64_t AllocateDocumentId()
    {
        auto value = next_document_id.load(std::memory_order_relaxed);
        for (;;)
        {
            // Leave max as a permanent exhausted sentinel instead of allowing
            // unsigned wraparound to make a stale identity valid again.
            if (value == 0 || value == std::numeric_limits<std::uint64_t>::max())
            {
                throw std::overflow_error("The process exhausted its document identities.");
            }

            if (next_document_id.compare_exchange_weak(
                value,
                value + 1,
                std::memory_order_relaxed,
                std::memory_order_relaxed))
            {
                return value;
            }
        }
    }
}

namespace octopaint::application
{
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

    DocumentMutation::DocumentMutation(void* const document) noexcept
        : document_(document)
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
        impl_->documents.push_back({
            .id = id,
            .document = core::Document{
                std::move(title),
                core::CanvasSize{ .width = size.width, .height = size.height } }
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

        DocumentMutation mutation{ &open_document->document };
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
        DocumentMutation mutation{ &open_document->document };
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
        DocumentMutation mutation{ &open_document->document };
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
            snapshot.documents.push_back({
                .id = open_document.id,
                .title = open_document.document.Title(),
                .canvas_size = { .width = size.width, .height = size.height },
                .is_active = impl_->active_document_id == open_document.id,
                .is_dirty = open_document.current_revision != open_document.saved_revision,
                .current_revision = open_document.current_revision,
                .saved_revision = open_document.saved_revision,
                .commands = std::move(commands)
            });
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
        return snapshot;
    }
}
