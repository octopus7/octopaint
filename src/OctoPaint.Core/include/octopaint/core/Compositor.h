#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <octopaint/core/Layer.h>

namespace octopaint::core
{
    enum class CompositeResult : std::uint8_t
    {
        Succeeded,
        InvalidOpacity,
        InvalidBlendMode,
        InvalidDestinationStride,
        InvalidSourceStride,
        DestinationBufferTooSmall,
        SourceBufferTooSmall,
        OverlappingBuffers,
        DestinationIsNotPremultiplied,
        SourceIsNotPremultiplied
    };

    [[nodiscard]] std::string_view CompositeResultMessage(CompositeResult result) noexcept;

    // Composites source over destination in place. Both buffers contain premultiplied BGRA8 pixels;
    // each color byte must be no greater than its pixel's alpha byte. Rows may have padding, but
    // pixels within a row are tightly packed. Empty width or height is a valid no-op.
    //
    // Color policy: blend functions operate directly on unpremultiplied 8-bit encoded channel
    // values (normally sRGB-encoded values). No transfer-function conversion or color-profile
    // transform is performed. This deliberately matches conventional encoded-RGB paint blending,
    // rather than linear-light compositing. All byte conversions use deterministic integer
    // round-to-nearest with half values rounded upward.
    //
    // Opacity must be finite and in [0, 1]. It is deterministically quantized to 16-bit UNORM.
    // All inputs are validated before destination is modified. Source may be exactly the same view
    // as destination; any other overlap is rejected because it cannot be composited safely in place.
    [[nodiscard]] CompositeResult CompositePremultipliedBgra8(
        std::span<std::byte> destination,
        std::size_t destination_stride_bytes,
        std::span<std::byte const> source,
        std::size_t source_stride_bytes,
        std::uint32_t width,
        std::uint32_t height,
        float opacity,
        BlendMode blend_mode) noexcept;
}
