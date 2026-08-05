#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace octopaint::application
{
    struct CanvasSize final
    {
        std::uint32_t width{};
        std::uint32_t height{};
    };

    struct WorkspaceSnapshot final
    {
        bool has_document{};
        std::string document_title;
        CanvasSize canvas_size;
        std::string status_message;
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

        void NewDocument(std::string title, CanvasSize size);
        [[nodiscard]] WorkspaceSnapshot Snapshot() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}

