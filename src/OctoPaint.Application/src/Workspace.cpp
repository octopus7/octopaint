#include <octopaint/application/Workspace.h>

#include <octopaint/core/Document.h>

#include <format>
#include <optional>
#include <utility>

namespace octopaint::application
{
    struct Workspace::Impl final
    {
        std::optional<core::Document> active_document;
    };

    Workspace::Workspace()
        : impl_(std::make_unique<Impl>())
    {
    }

    Workspace::~Workspace() = default;
    Workspace::Workspace(Workspace&&) noexcept = default;
    Workspace& Workspace::operator=(Workspace&&) noexcept = default;

    void Workspace::NewDocument(std::string title, CanvasSize const size)
    {
        impl_->active_document.emplace(
            std::move(title),
            core::CanvasSize{ .width = size.width, .height = size.height });
    }

    WorkspaceSnapshot Workspace::Snapshot() const
    {
        if (!impl_->active_document)
        {
            return {
                .has_document = false,
                .document_title = "No document",
                .canvas_size = {},
                .status_message = "Ready"
            };
        }

        auto const& document = *impl_->active_document;
        auto const size = document.Size();

        return {
            .has_document = true,
            .document_title = document.Title(),
            .canvas_size = { .width = size.width, .height = size.height },
            .status_message = std::format("{} x {} pixels", size.width, size.height)
        };
    }
}

