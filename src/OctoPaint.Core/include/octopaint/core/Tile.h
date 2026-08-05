#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace octopaint::core
{
    inline constexpr std::uint32_t TileExtent = 256;
    inline constexpr std::uint32_t Rgba8BytesPerPixel = 4;
    inline constexpr std::size_t Rgba8TileByteCount =
        static_cast<std::size_t>(TileExtent) * TileExtent * Rgba8BytesPerPixel;

    enum class TileFormat : std::uint8_t
    {
        Rgba8Premultiplied
    };

    struct TileKey final
    {
        std::int32_t x{};
        std::int32_t y{};
        std::uint32_t level{};

        auto operator<=>(TileKey const&) const = default;
    };

    class TilePayload final
    {
    public:
        [[nodiscard]] static TilePayload FromRgba8(std::span<std::byte const> pixels);

        [[nodiscard]] TileFormat Format() const noexcept;
        [[nodiscard]] std::span<std::byte const> Pixels() const noexcept;
        [[nodiscard]] std::size_t ByteSize() const noexcept;
        [[nodiscard]] bool IsFullyTransparent() const noexcept;
        [[nodiscard]] bool SharesStorageWith(TilePayload const& other) const noexcept;

    private:
        friend class TileDraft;
        explicit TilePayload(std::shared_ptr<std::vector<std::byte> const> pixels) noexcept;

        std::shared_ptr<std::vector<std::byte> const> pixels_;
    };

    class TileDraft final
    {
    public:
        [[nodiscard]] std::span<std::byte> Pixels() noexcept;
        [[nodiscard]] std::span<std::byte const> Pixels() const noexcept;
        [[nodiscard]] bool IsFullyTransparent() const noexcept;

    private:
        friend class SparseTileStore;

        explicit TileDraft(std::vector<std::byte> pixels) noexcept;
        [[nodiscard]] TilePayload Freeze() &&;

        std::vector<std::byte> pixels_;
    };

    enum class TilePublishResult : std::uint8_t
    {
        Stored,
        RemovedTransparent
    };

    class SparseTileStore final
    {
    public:
        SparseTileStore() = default;
        SparseTileStore(SparseTileStore const&) = default;
        SparseTileStore(SparseTileStore&&) noexcept = default;
        SparseTileStore& operator=(SparseTileStore const&) = default;
        SparseTileStore& operator=(SparseTileStore&&) noexcept = default;

        [[nodiscard]] std::optional<TilePayload> Read(TileKey key) const;
        [[nodiscard]] TileDraft BeginWrite(TileKey key) const;
        [[nodiscard]] TilePublishResult Publish(TileKey key, TileDraft draft);
        [[nodiscard]] TilePublishResult Publish(TileKey key, TilePayload payload);
        [[nodiscard]] bool Erase(TileKey key) noexcept;

        [[nodiscard]] bool Empty() const noexcept;
        [[nodiscard]] std::size_t TileCount() const noexcept;
        [[nodiscard]] std::vector<TileKey> Keys() const;

    private:
        std::map<TileKey, TilePayload> tiles_;
    };
}
