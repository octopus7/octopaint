#pragma once

#include <compare>
#include <cstdint>
#include <string>

namespace octopaint::core
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

    class Document final
    {
    public:
        Document(std::string title, CanvasSize size);

        [[nodiscard]] std::string const& Title() const noexcept;
        [[nodiscard]] CanvasSize Size() const noexcept;

        void Rename(std::string title);

    private:
        std::string title_;
        CanvasSize size_;
    };
}

