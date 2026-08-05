#pragma once

#include <compare>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace octopaint::application
{
    struct CanvasSize final
    {
        std::uint32_t width{};
        std::uint32_t height{};

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return width > 0 && height > 0;
        }

        auto operator<=>(CanvasSize const&) const = default;
    };

    // A process-unique, strongly typed document identity. A closed document's
    // identity is never reused, so stale UI state cannot target a new document.
    class DocumentId final
    {
    public:
        constexpr DocumentId() noexcept = default;

        [[nodiscard]] constexpr std::uint64_t Value() const noexcept
        {
            return value_;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return value_ != 0;
        }

        auto operator<=>(DocumentId const&) const = default;

    private:
        explicit constexpr DocumentId(std::uint64_t const value) noexcept
            : value_(value)
        {
        }

        std::uint64_t value_{};

        friend class Workspace;
    };

    // The deliberately small mutation boundary exposed to application commands.
    // UI events should create a command and submit it to Workspace instead of
    // mutating documents directly.
    class DocumentMutation final
    {
    public:
        [[nodiscard]] std::string const& Title() const noexcept;
        [[nodiscard]] CanvasSize Size() const noexcept;
        void Rename(std::string title);

    private:
        explicit DocumentMutation(void* document) noexcept;

        void* document_{};

        friend class Workspace;
    };

    class DocumentCommand
    {
    public:
        virtual ~DocumentCommand() = default;

        [[nodiscard]] virtual std::string Label() const = 0;
        virtual void Execute(DocumentMutation& document) = 0;
        virtual void Undo(DocumentMutation& document) = 0;
    };

    class RenameDocumentCommand final : public DocumentCommand
    {
    public:
        explicit RenameDocumentCommand(std::string new_title);

        [[nodiscard]] std::string Label() const override;
        void Execute(DocumentMutation& document) override;
        void Undo(DocumentMutation& document) override;

    private:
        std::string new_title_;
        std::string previous_title_;
        bool captured_previous_title_{};
    };

    struct CommandAvailability final
    {
        bool can_undo{};
        bool can_redo{};
        std::string undo_label;
        std::string redo_label;
    };

    struct DocumentSummary final
    {
        DocumentId id;
        std::string title;
        CanvasSize canvas_size;
        bool is_active{};
        bool is_dirty{};
        std::uint64_t current_revision{};
        std::uint64_t saved_revision{};
        CommandAvailability commands;
    };

    // This is a detached value snapshot: modifying the returned values never
    // mutates Workspace. The four leading fields preserve the original API.
    struct WorkspaceSnapshot final
    {
        bool has_document{};
        std::string document_title;
        CanvasSize canvas_size;
        std::string status_message;

        std::vector<DocumentSummary> documents;
        std::optional<DocumentId> active_document_id;
        CommandAvailability active_commands;
    };

    class Workspace final
    {
    public:
        Workspace();
        ~Workspace();

        Workspace(Workspace&&) noexcept;
        Workspace& operator=(Workspace&&) noexcept;

        Workspace(Workspace const&) = delete;
        Workspace& operator=(Workspace const&) = delete;

        // Source-compatible migration: existing callers may ignore the new ID.
        DocumentId NewDocument(std::string title, CanvasSize size);
        [[nodiscard]] bool CloseDocument(DocumentId id);
        [[nodiscard]] bool ActivateDocument(DocumentId id);
        [[nodiscard]] std::optional<DocumentId> ActiveDocument() const noexcept;
        [[nodiscard]] bool Contains(DocumentId id) const noexcept;

        void ExecuteCommand(DocumentId id, std::unique_ptr<DocumentCommand> command);
        [[nodiscard]] bool Undo(DocumentId id);
        [[nodiscard]] bool Redo(DocumentId id);
        void MarkSaved(DocumentId id);

        void ExecuteCommand(std::unique_ptr<DocumentCommand> command);
        [[nodiscard]] bool Undo();
        [[nodiscard]] bool Redo();
        void MarkActiveDocumentSaved();

        [[nodiscard]] WorkspaceSnapshot Snapshot() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}
