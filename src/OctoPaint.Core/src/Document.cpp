#include <octopaint/core/Document.h>

#include <stdexcept>
#include <utility>

namespace octopaint::core
{
    Document::Document(std::string title, CanvasSize const size)
        : title_(std::move(title)), size_(size)
    {
        if (title_.empty())
        {
            throw std::invalid_argument("A document title cannot be empty.");
        }

        if (!size_.IsValid())
        {
            throw std::invalid_argument("Canvas dimensions must be greater than zero.");
        }
    }

    std::string const& Document::Title() const noexcept
    {
        return title_;
    }

    CanvasSize Document::Size() const noexcept
    {
        return size_;
    }

    void Document::Rename(std::string title)
    {
        if (title.empty())
        {
            throw std::invalid_argument("A document title cannot be empty.");
        }

        title_ = std::move(title);
    }
}

