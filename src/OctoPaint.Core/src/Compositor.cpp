#include <octopaint/core/Compositor.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>

namespace octopaint::core
{
    namespace
    {
        constexpr std::uint64_t ByteMaximum = 255;
        constexpr std::uint64_t OpacityMaximum = 65535;
        constexpr std::size_t BytesPerPixel = 4;
        constexpr std::int64_t ColorMaximum = 65535;

        struct Color final
        {
            std::int64_t red{};
            std::int64_t green{};
            std::int64_t blue{};
        };

        [[nodiscard]] constexpr std::uint64_t DivideRounded(
            std::uint64_t const numerator,
            std::uint64_t const denominator) noexcept
        {
            return (numerator + denominator / 2) / denominator;
        }

        [[nodiscard]] constexpr std::int64_t DivideRoundedSigned(
            std::int64_t const numerator,
            std::int64_t const denominator) noexcept
        {
            if (numerator >= 0)
            {
                return (numerator + denominator / 2) / denominator;
            }

            return -((-numerator + denominator / 2) / denominator);
        }

        [[nodiscard]] constexpr std::uint8_t ToByte(std::byte const value) noexcept
        {
            return std::to_integer<std::uint8_t>(value);
        }

        [[nodiscard]] constexpr std::uint8_t ClampByte(std::uint64_t const value) noexcept
        {
            return static_cast<std::uint8_t>(std::min(value, ByteMaximum));
        }

        [[nodiscard]] constexpr std::uint8_t Unpremultiply(
            std::uint8_t const channel,
            std::uint8_t const alpha) noexcept
        {
            if (alpha == 0)
            {
                return 0;
            }

            return ClampByte(DivideRounded(
                static_cast<std::uint64_t>(channel) * ByteMaximum,
                alpha));
        }

        [[nodiscard]] constexpr std::uint8_t Multiply(
            std::uint8_t const backdrop,
            std::uint8_t const source) noexcept
        {
            return static_cast<std::uint8_t>(DivideRounded(
                static_cast<std::uint64_t>(backdrop) * source,
                ByteMaximum));
        }

        [[nodiscard]] constexpr std::uint8_t Screen(
            std::uint8_t const backdrop,
            std::uint8_t const source) noexcept
        {
            return static_cast<std::uint8_t>(ByteMaximum - DivideRounded(
                (ByteMaximum - backdrop) * (ByteMaximum - source),
                ByteMaximum));
        }

        [[nodiscard]] constexpr std::uint64_t IntegerSquareRoot(std::uint64_t value) noexcept
        {
            std::uint64_t result = 0;
            std::uint64_t bit = std::uint64_t{ 1 } << 62;
            while (bit > value)
            {
                bit >>= 2;
            }

            while (bit != 0)
            {
                if (value >= result + bit)
                {
                    value -= result + bit;
                    result = (result >> 1) + bit;
                }
                else
                {
                    result >>= 1;
                }
                bit >>= 2;
            }

            return result;
        }

        [[nodiscard]] constexpr std::uint8_t SoftLightCurve(std::uint8_t const backdrop) noexcept
        {
            if (backdrop <= 63)
            {
                auto const b = static_cast<std::uint64_t>(backdrop);
                auto const numerator = 16 * b * b * b
                    + 4 * b * ByteMaximum * ByteMaximum
                    - 12 * b * b * ByteMaximum;
                return ClampByte(DivideRounded(numerator, ByteMaximum * ByteMaximum));
            }

            auto const radicand = static_cast<std::uint64_t>(backdrop) * ByteMaximum;
            auto const root = IntegerSquareRoot(radicand);
            auto const midpoint_twice = 2 * root + 1;
            return static_cast<std::uint8_t>(
                4 * radicand >= midpoint_twice * midpoint_twice ? root + 1 : root);
        }

        [[nodiscard]] constexpr std::uint8_t SoftLight(
            std::uint8_t const backdrop,
            std::uint8_t const source) noexcept
        {
            auto const b = static_cast<std::uint64_t>(backdrop);
            if (source <= 127)
            {
                auto const numerator = b * ByteMaximum * ByteMaximum
                    - (ByteMaximum - 2 * source) * b * (ByteMaximum - b);
                return ClampByte(DivideRounded(numerator, ByteMaximum * ByteMaximum));
            }

            auto const curve = static_cast<std::int64_t>(SoftLightCurve(backdrop));
            auto const numerator = static_cast<std::int64_t>(backdrop) * ByteMaximum
                + static_cast<std::int64_t>(2 * source - ByteMaximum)
                    * (curve - backdrop);
            return static_cast<std::uint8_t>(std::clamp<std::int64_t>(
                DivideRoundedSigned(numerator, ByteMaximum), 0, ByteMaximum));
        }

        [[nodiscard]] constexpr std::int64_t Luminosity(Color const color) noexcept
        {
            return DivideRoundedSigned(
                30 * color.red + 59 * color.green + 11 * color.blue,
                100);
        }

        [[nodiscard]] constexpr std::int64_t Saturation(Color const color) noexcept
        {
            return std::max({ color.red, color.green, color.blue })
                - std::min({ color.red, color.green, color.blue });
        }

        [[nodiscard]] constexpr Color ClipColor(Color color) noexcept
        {
            auto const luminosity = Luminosity(color);
            auto const minimum = std::min({ color.red, color.green, color.blue });
            if (minimum < 0)
            {
                auto const scale = luminosity - minimum;
                auto clip = [luminosity, scale](std::int64_t const channel) constexpr noexcept
                {
                    return luminosity + DivideRoundedSigned(
                        (channel - luminosity) * luminosity,
                        scale);
                };
                color = { clip(color.red), clip(color.green), clip(color.blue) };
            }

            auto const maximum = std::max({ color.red, color.green, color.blue });
            if (maximum > ColorMaximum)
            {
                auto const scale = maximum - luminosity;
                auto clip = [luminosity, scale](std::int64_t const channel) constexpr noexcept
                {
                    return luminosity + DivideRoundedSigned(
                        (channel - luminosity) * (ColorMaximum - luminosity),
                        scale);
                };
                color = { clip(color.red), clip(color.green), clip(color.blue) };
            }

            color.red = std::clamp<std::int64_t>(color.red, 0, ColorMaximum);
            color.green = std::clamp<std::int64_t>(color.green, 0, ColorMaximum);
            color.blue = std::clamp<std::int64_t>(color.blue, 0, ColorMaximum);
            return color;
        }

        [[nodiscard]] constexpr Color SetLuminosity(
            Color color,
            std::int64_t const luminosity) noexcept
        {
            auto const difference = luminosity - Luminosity(color);
            color.red += difference;
            color.green += difference;
            color.blue += difference;
            return ClipColor(color);
        }

        [[nodiscard]] constexpr Color SetSaturation(
            Color const color,
            std::int64_t const saturation) noexcept
        {
            std::array<std::pair<std::int64_t, std::size_t>, 3> channels{
                std::pair{ color.red, std::size_t{ 0 } },
                std::pair{ color.green, std::size_t{ 1 } },
                std::pair{ color.blue, std::size_t{ 2 } }
            };
            std::sort(channels.begin(), channels.end());

            std::array<std::int64_t, 3> result{};
            auto const range = channels[2].first - channels[0].first;
            if (range != 0)
            {
                result[channels[1].second] = DivideRoundedSigned(
                    (channels[1].first - channels[0].first) * saturation,
                    range);
                result[channels[2].second] = saturation;
            }

            return Color{ result[0], result[1], result[2] };
        }

        [[nodiscard]] constexpr Color ToColor(
            std::uint8_t const red,
            std::uint8_t const green,
            std::uint8_t const blue) noexcept
        {
            return Color{
                static_cast<std::int64_t>(red) * 257,
                static_cast<std::int64_t>(green) * 257,
                static_cast<std::int64_t>(blue) * 257
            };
        }

        [[nodiscard]] constexpr std::array<std::uint8_t, 3> ToBytes(Color const color) noexcept
        {
            return {
                static_cast<std::uint8_t>(std::clamp<std::int64_t>(
                    DivideRoundedSigned(color.red, 257), 0, ByteMaximum)),
                static_cast<std::uint8_t>(std::clamp<std::int64_t>(
                    DivideRoundedSigned(color.green, 257), 0, ByteMaximum)),
                static_cast<std::uint8_t>(std::clamp<std::int64_t>(
                    DivideRoundedSigned(color.blue, 257), 0, ByteMaximum))
            };
        }

        [[nodiscard]] constexpr std::uint8_t BlendChannel(
            std::uint8_t const backdrop,
            std::uint8_t const source,
            BlendMode const mode) noexcept
        {
            switch (mode)
            {
            case BlendMode::Normal:
                return source;
            case BlendMode::Multiply:
                return Multiply(backdrop, source);
            case BlendMode::Screen:
                return Screen(backdrop, source);
            case BlendMode::Overlay:
                return backdrop <= 127
                    ? ClampByte(DivideRounded(2ULL * backdrop * source, ByteMaximum))
                    : static_cast<std::uint8_t>(ByteMaximum - DivideRounded(
                        2ULL * (ByteMaximum - backdrop) * (ByteMaximum - source),
                        ByteMaximum));
            case BlendMode::Darken:
                return std::min(backdrop, source);
            case BlendMode::Lighten:
                return std::max(backdrop, source);
            case BlendMode::ColorDodge:
                return source == ByteMaximum
                    ? static_cast<std::uint8_t>(ByteMaximum)
                    : ClampByte(DivideRounded(
                        static_cast<std::uint64_t>(backdrop) * ByteMaximum,
                        ByteMaximum - source));
            case BlendMode::ColorBurn:
                return source == 0
                    ? 0
                    : static_cast<std::uint8_t>(ByteMaximum - std::min(
                        ByteMaximum,
                        DivideRounded((ByteMaximum - backdrop) * ByteMaximum, source)));
            case BlendMode::SoftLight:
                return SoftLight(backdrop, source);
            case BlendMode::HardLight:
                return source <= 127
                    ? ClampByte(DivideRounded(2ULL * backdrop * source, ByteMaximum))
                    : static_cast<std::uint8_t>(ByteMaximum - DivideRounded(
                        2ULL * (ByteMaximum - backdrop) * (ByteMaximum - source),
                        ByteMaximum));
            case BlendMode::Difference:
                return static_cast<std::uint8_t>(backdrop > source
                    ? backdrop - source
                    : source - backdrop);
            case BlendMode::Exclusion:
                return ClampByte(DivideRounded(
                    static_cast<std::uint64_t>(backdrop) * ByteMaximum
                        + static_cast<std::uint64_t>(source) * ByteMaximum
                        - 2ULL * backdrop * source,
                    ByteMaximum));
            default:
                return 0;
            }
        }

        [[nodiscard]] constexpr std::array<std::uint8_t, 3> BlendColor(
            std::array<std::uint8_t, 3> const backdrop,
            std::array<std::uint8_t, 3> const source,
            BlendMode const mode) noexcept
        {
            if (mode <= BlendMode::Exclusion)
            {
                return {
                    BlendChannel(backdrop[0], source[0], mode),
                    BlendChannel(backdrop[1], source[1], mode),
                    BlendChannel(backdrop[2], source[2], mode)
                };
            }

            auto const backdrop_color = ToColor(backdrop[0], backdrop[1], backdrop[2]);
            auto const source_color = ToColor(source[0], source[1], source[2]);
            switch (mode)
            {
            case BlendMode::Hue:
                return ToBytes(SetLuminosity(
                    SetSaturation(source_color, Saturation(backdrop_color)),
                    Luminosity(backdrop_color)));
            case BlendMode::Saturation:
                return ToBytes(SetLuminosity(
                    SetSaturation(backdrop_color, Saturation(source_color)),
                    Luminosity(backdrop_color)));
            case BlendMode::Color:
                return ToBytes(SetLuminosity(source_color, Luminosity(backdrop_color)));
            case BlendMode::Luminosity:
                return ToBytes(SetLuminosity(backdrop_color, Luminosity(source_color)));
            default:
                return {};
            }
        }

        [[nodiscard]] constexpr bool IsBlendModeValid(BlendMode const mode) noexcept
        {
            return mode >= BlendMode::Normal && mode <= BlendMode::Luminosity;
        }

        [[nodiscard]] bool RequiredBufferSize(
            std::uint32_t const width,
            std::uint32_t const height,
            std::size_t const stride,
            std::size_t& required) noexcept
        {
            if (width == 0 || height == 0)
            {
                required = 0;
                return true;
            }

            if (width > std::numeric_limits<std::size_t>::max() / BytesPerPixel)
            {
                return false;
            }
            auto const row_bytes = static_cast<std::size_t>(width) * BytesPerPixel;
            if (stride < row_bytes)
            {
                return false;
            }

            auto const preceding_rows = static_cast<std::size_t>(height - 1);
            if (preceding_rows > (std::numeric_limits<std::size_t>::max() - row_bytes) / stride)
            {
                return false;
            }
            required = preceding_rows * stride + row_bytes;
            return true;
        }

        [[nodiscard]] bool RangesOverlap(
            std::span<std::byte> const destination,
            std::span<std::byte const> const source) noexcept
        {
            if (destination.empty() || source.empty())
            {
                return false;
            }

            auto const destination_begin = reinterpret_cast<std::uintptr_t>(destination.data());
            auto const source_begin = reinterpret_cast<std::uintptr_t>(source.data());
            auto const destination_end = destination_begin + destination.size();
            auto const source_end = source_begin + source.size();
            return destination_begin < source_end && source_begin < destination_end;
        }

        [[nodiscard]] bool IsPremultiplied(
            std::span<std::byte const> const pixels,
            std::size_t const stride,
            std::uint32_t const width,
            std::uint32_t const height) noexcept
        {
            for (std::uint32_t y = 0; y < height; ++y)
            {
                auto const row = pixels.subspan(static_cast<std::size_t>(y) * stride);
                for (std::uint32_t x = 0; x < width; ++x)
                {
                    auto const offset = static_cast<std::size_t>(x) * BytesPerPixel;
                    auto const alpha = ToByte(row[offset + 3]);
                    if (ToByte(row[offset]) > alpha
                        || ToByte(row[offset + 1]) > alpha
                        || ToByte(row[offset + 2]) > alpha)
                    {
                        return false;
                    }
                }
            }
            return true;
        }

        void CompositePixel(
            std::span<std::byte> const destination,
            std::span<std::byte const> const source,
            std::uint64_t const opacity,
            BlendMode const blend_mode) noexcept
        {
            auto const source_alpha = ToByte(source[3]);
            auto const destination_alpha = ToByte(destination[3]);
            auto const inverse_source_alpha = ByteMaximum * OpacityMaximum
                - static_cast<std::uint64_t>(source_alpha) * opacity;

            if (blend_mode == BlendMode::Normal)
            {
                auto const denominator = ByteMaximum * OpacityMaximum;
                for (std::size_t channel = 0; channel < 3; ++channel)
                {
                    auto const numerator = static_cast<std::uint64_t>(ToByte(destination[channel]))
                            * inverse_source_alpha
                        + static_cast<std::uint64_t>(ToByte(source[channel])) * opacity
                            * ByteMaximum;
                    destination[channel] = static_cast<std::byte>(ClampByte(
                        DivideRounded(numerator, denominator)));
                }

                auto const alpha_numerator = static_cast<std::uint64_t>(source_alpha) * opacity
                        * ByteMaximum
                    + static_cast<std::uint64_t>(destination_alpha) * inverse_source_alpha;
                destination[3] = static_cast<std::byte>(ClampByte(DivideRounded(
                    alpha_numerator,
                    denominator)));
                return;
            }

            auto const source_color = std::array{
                Unpremultiply(ToByte(source[2]), source_alpha),
                Unpremultiply(ToByte(source[1]), source_alpha),
                Unpremultiply(ToByte(source[0]), source_alpha)
            };
            auto const destination_color = std::array{
                Unpremultiply(ToByte(destination[2]), destination_alpha),
                Unpremultiply(ToByte(destination[1]), destination_alpha),
                Unpremultiply(ToByte(destination[0]), destination_alpha)
            };
            auto const blended = BlendColor(destination_color, source_color, blend_mode);

            auto const color_denominator = ByteMaximum * ByteMaximum * OpacityMaximum;
            for (std::size_t channel = 0; channel < 3; ++channel)
            {
                auto const bgra_channel = 2 - channel;
                auto const numerator = static_cast<std::uint64_t>(ToByte(destination[bgra_channel]))
                        * inverse_source_alpha * ByteMaximum
                    + static_cast<std::uint64_t>(ToByte(source[bgra_channel])) * opacity
                        * (ByteMaximum - destination_alpha) * ByteMaximum
                    + static_cast<std::uint64_t>(source_alpha) * opacity * destination_alpha
                        * blended[channel];
                destination[bgra_channel] = static_cast<std::byte>(ClampByte(
                    DivideRounded(numerator, color_denominator)));
            }

            auto const alpha_numerator = static_cast<std::uint64_t>(source_alpha) * opacity
                    * ByteMaximum
                + static_cast<std::uint64_t>(destination_alpha) * inverse_source_alpha;
            destination[3] = static_cast<std::byte>(ClampByte(DivideRounded(
                alpha_numerator,
                ByteMaximum * OpacityMaximum)));
        }
    }

    std::string_view CompositeResultMessage(CompositeResult const result) noexcept
    {
        switch (result)
        {
        case CompositeResult::Succeeded:
            return "The buffers were composited successfully.";
        case CompositeResult::InvalidOpacity:
            return "Opacity must be finite and between zero and one.";
        case CompositeResult::InvalidBlendMode:
            return "The blend mode is not supported.";
        case CompositeResult::InvalidDestinationStride:
            return "The destination stride is smaller than one pixel row or overflows the buffer size.";
        case CompositeResult::InvalidSourceStride:
            return "The source stride is smaller than one pixel row or overflows the buffer size.";
        case CompositeResult::DestinationBufferTooSmall:
            return "The destination buffer does not contain all requested rows.";
        case CompositeResult::SourceBufferTooSmall:
            return "The source buffer does not contain all requested rows.";
        case CompositeResult::OverlappingBuffers:
            return "Source and destination overlap but are not the same image view.";
        case CompositeResult::DestinationIsNotPremultiplied:
            return "A destination color channel is greater than its alpha channel.";
        case CompositeResult::SourceIsNotPremultiplied:
            return "A source color channel is greater than its alpha channel.";
        default:
            return "Unknown compositor result.";
        }
    }

    CompositeResult CompositePremultipliedBgra8(
        std::span<std::byte> const destination,
        std::size_t const destination_stride_bytes,
        std::span<std::byte const> const source,
        std::size_t const source_stride_bytes,
        std::uint32_t const width,
        std::uint32_t const height,
        float const opacity,
        BlendMode const blend_mode) noexcept
    {
        if (!std::isfinite(opacity) || opacity < 0.0F || opacity > 1.0F)
        {
            return CompositeResult::InvalidOpacity;
        }
        if (!IsBlendModeValid(blend_mode))
        {
            return CompositeResult::InvalidBlendMode;
        }
        if (width == 0 || height == 0)
        {
            return CompositeResult::Succeeded;
        }

        std::size_t destination_required{};
        if (!RequiredBufferSize(width, height, destination_stride_bytes, destination_required))
        {
            return CompositeResult::InvalidDestinationStride;
        }
        std::size_t source_required{};
        if (!RequiredBufferSize(width, height, source_stride_bytes, source_required))
        {
            return CompositeResult::InvalidSourceStride;
        }
        if (destination.size() < destination_required)
        {
            return CompositeResult::DestinationBufferTooSmall;
        }
        if (source.size() < source_required)
        {
            return CompositeResult::SourceBufferTooSmall;
        }

        auto const same_view = destination.data() == source.data()
            && destination_stride_bytes == source_stride_bytes;
        if (!same_view && RangesOverlap(destination, source))
        {
            return CompositeResult::OverlappingBuffers;
        }
        if (!IsPremultiplied(destination, destination_stride_bytes, width, height))
        {
            return CompositeResult::DestinationIsNotPremultiplied;
        }
        if (!IsPremultiplied(source, source_stride_bytes, width, height))
        {
            return CompositeResult::SourceIsNotPremultiplied;
        }

        auto const quantized_opacity = static_cast<std::uint64_t>(
            static_cast<double>(opacity) * OpacityMaximum + 0.5);
        if (quantized_opacity == 0)
        {
            return CompositeResult::Succeeded;
        }

        for (std::uint32_t y = 0; y < height; ++y)
        {
            auto const destination_row = destination.subspan(
                static_cast<std::size_t>(y) * destination_stride_bytes);
            auto const source_row = source.subspan(
                static_cast<std::size_t>(y) * source_stride_bytes);
            for (std::uint32_t x = 0; x < width; ++x)
            {
                auto const offset = static_cast<std::size_t>(x) * BytesPerPixel;
                CompositePixel(
                    destination_row.subspan(offset, BytesPerPixel),
                    source_row.subspan(offset, BytesPerPixel),
                    quantized_opacity,
                    blend_mode);
            }
        }

        return CompositeResult::Succeeded;
    }
}
