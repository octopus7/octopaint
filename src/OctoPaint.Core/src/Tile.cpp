#include <octopaint/core/Tile.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace octopaint::core
{
    namespace
    {
        [[nodiscard]] bool IsFullyTransparent(std::span<std::byte const> const pixels) noexcept
        {
            for (std::size_t index = 3; index < pixels.size(); index += Rgba8BytesPerPixel)
            {
                if (pixels[index] != std::byte{ 0 })
                {
                    return false;
                }
            }

            return true;
        }
    }

    TilePayload TilePayload::FromRgba8(std::span<std::byte const> const pixels)
    {
        if (pixels.size() != Rgba8TileByteCount)
        {
            throw std::invalid_argument("An RGBA8 tile payload must contain exactly 256 x 256 x 4 bytes.");
        }

        return TilePayload(std::make_shared<std::vector<std::byte> const>(pixels.begin(), pixels.end()));
    }

    TilePayload::TilePayload(std::shared_ptr<std::vector<std::byte> const> pixels) noexcept
        : pixels_(std::move(pixels))
    {
    }

    TileFormat TilePayload::Format() const noexcept
    {
        return TileFormat::Rgba8Premultiplied;
    }

    std::span<std::byte const> TilePayload::Pixels() const noexcept
    {
        return *pixels_;
    }

    std::size_t TilePayload::ByteSize() const noexcept
    {
        return pixels_->size();
    }

    bool TilePayload::IsFullyTransparent() const noexcept
    {
        return octopaint::core::IsFullyTransparent(Pixels());
    }

    bool TilePayload::SharesStorageWith(TilePayload const& other) const noexcept
    {
        return pixels_.get() == other.pixels_.get();
    }

    TileDraft::TileDraft(std::vector<std::byte> pixels) noexcept
        : pixels_(std::move(pixels))
    {
    }

    std::span<std::byte> TileDraft::Pixels() noexcept
    {
        return pixels_;
    }

    std::span<std::byte const> TileDraft::Pixels() const noexcept
    {
        return pixels_;
    }

    bool TileDraft::IsFullyTransparent() const noexcept
    {
        return octopaint::core::IsFullyTransparent(Pixels());
    }

    TilePayload TileDraft::Freeze() &&
    {
        return TilePayload(std::make_shared<std::vector<std::byte> const>(std::move(pixels_)));
    }

    std::optional<TilePayload> SparseTileStore::Read(TileKey const key) const
    {
        auto const iterator = tiles_.find(key);
        if (iterator == tiles_.end())
        {
            return std::nullopt;
        }

        return iterator->second;
    }

    TileDraft SparseTileStore::BeginWrite(TileKey const key) const
    {
        auto const payload = Read(key);
        if (!payload)
        {
            return TileDraft(std::vector<std::byte>(Rgba8TileByteCount));
        }

        auto const pixels = payload->Pixels();
        return TileDraft(std::vector<std::byte>(pixels.begin(), pixels.end()));
    }

    TilePublishResult SparseTileStore::Publish(TileKey const key, TileDraft draft)
    {
        if (draft.IsFullyTransparent())
        {
            tiles_.erase(key);
            return TilePublishResult::RemovedTransparent;
        }

        tiles_.insert_or_assign(key, std::move(draft).Freeze());
        return TilePublishResult::Stored;
    }

    TilePublishResult SparseTileStore::Publish(TileKey const key, TilePayload payload)
    {
        if (payload.IsFullyTransparent())
        {
            tiles_.erase(key);
            return TilePublishResult::RemovedTransparent;
        }

        tiles_.insert_or_assign(key, std::move(payload));
        return TilePublishResult::Stored;
    }

    bool SparseTileStore::Erase(TileKey const key) noexcept
    {
        return tiles_.erase(key) != 0;
    }

    bool SparseTileStore::Empty() const noexcept
    {
        return tiles_.empty();
    }

    std::size_t SparseTileStore::TileCount() const noexcept
    {
        return tiles_.size();
    }

    std::vector<TileKey> SparseTileStore::Keys() const
    {
        std::vector<TileKey> keys;
        keys.reserve(tiles_.size());
        for (auto const& [key, payload] : tiles_)
        {
            static_cast<void>(payload);
            keys.push_back(key);
        }

        return keys;
    }
}
