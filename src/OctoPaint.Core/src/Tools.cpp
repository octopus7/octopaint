#include <octopaint/core/Tools.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>

namespace octopaint::core
{
    struct SelectionMaskAccess final
    {
        [[nodiscard]] static SelectionMask Create(RectI const bounds, std::vector<std::uint8_t> coverage) noexcept
        {
            return SelectionMask(bounds, std::move(coverage));
        }
    };

    namespace
    {
        constexpr std::uint64_t MaxGeneratedElements = 16'777'216;

        [[nodiscard]] RectI Intersect(RectI const left, RectI const right) noexcept
        {
            auto const x0 = std::max<std::int64_t>(left.x, right.x);
            auto const y0 = std::max<std::int64_t>(left.y, right.y);
            auto const x1 = std::min<std::int64_t>(
                static_cast<std::int64_t>(left.x) + left.width,
                static_cast<std::int64_t>(right.x) + right.width);
            auto const y1 = std::min<std::int64_t>(
                static_cast<std::int64_t>(left.y) + left.height,
                static_cast<std::int64_t>(right.y) + right.height);
            if (x1 <= x0 || y1 <= y0)
            {
                return {};
            }

            return {
                static_cast<std::int32_t>(x0),
                static_cast<std::int32_t>(y0),
                static_cast<std::int32_t>(x1 - x0),
                static_cast<std::int32_t>(y1 - y0)
            };
        }

        void ValidateBounds(RectI const bounds)
        {
            if (!bounds.IsValid())
            {
                throw std::invalid_argument("Bounds must have positive dimensions without coordinate overflow.");
            }

            auto const area = static_cast<std::uint64_t>(bounds.width) * static_cast<std::uint64_t>(bounds.height);
            if (area > MaxGeneratedElements)
            {
                throw std::length_error("A rasterized result cannot exceed the deterministic element limit.");
            }
        }

        [[nodiscard]] std::size_t PixelIndex(RectI const bounds, PointI const point) noexcept
        {
            auto const row = static_cast<std::size_t>(static_cast<std::int64_t>(point.y) - bounds.y);
            auto const column = static_cast<std::size_t>(static_cast<std::int64_t>(point.x) - bounds.x);
            return row * static_cast<std::size_t>(bounds.width) + column;
        }

        [[nodiscard]] SelectionMask MakeMask(RectI const bounds, auto&& predicate)
        {
            if (!bounds.IsValid())
            {
                return {};
            }

            auto const area = static_cast<std::uint64_t>(bounds.width) * static_cast<std::uint64_t>(bounds.height);
            if (area > MaxGeneratedElements)
            {
                throw std::length_error("A selection mask cannot exceed the deterministic element limit.");
            }

            std::vector<std::uint8_t> coverage(static_cast<std::size_t>(area));
            for (std::int64_t y = bounds.y; y < static_cast<std::int64_t>(bounds.y) + bounds.height; ++y)
            {
                for (std::int64_t x = bounds.x; x < static_cast<std::int64_t>(bounds.x) + bounds.width; ++x)
                {
                    PointI const point{ static_cast<std::int32_t>(x), static_cast<std::int32_t>(y) };
                    coverage[PixelIndex(bounds, point)] = predicate(point) ? 1U : 0U;
                }
            }

            return SelectionMaskAccess::Create(bounds, std::move(coverage));
        }

        [[nodiscard]] bool PointOnSegment(PointI const point, PointI const start, PointI const end) noexcept
        {
            auto const px = static_cast<long double>(point.x);
            auto const py = static_cast<long double>(point.y);
            auto const ax = static_cast<long double>(start.x);
            auto const ay = static_cast<long double>(start.y);
            auto const bx = static_cast<long double>(end.x);
            auto const by = static_cast<long double>(end.y);
            auto const cross = (px - ax) * (by - ay) - (py - ay) * (bx - ax);
            if (cross != 0.0L)
            {
                return false;
            }

            return px >= std::min(ax, bx) && px <= std::max(ax, bx)
                && py >= std::min(ay, by) && py <= std::max(ay, by);
        }

        [[nodiscard]] bool PointInClosedPath(PointI const point, std::span<PointI const> const points) noexcept
        {
            bool inside = false;
            for (std::size_t current = 0, previous = points.size() - 1; current < points.size(); previous = current++)
            {
                auto const a = points[previous];
                auto const b = points[current];
                if (PointOnSegment(point, a, b))
                {
                    return true;
                }

                auto const crosses_y = (a.y > point.y) != (b.y > point.y);
                if (crosses_y)
                {
                    auto const intersection_x = static_cast<long double>(b.x - a.x)
                        * static_cast<long double>(point.y - a.y)
                        / static_cast<long double>(b.y - a.y)
                        + a.x;
                    if (static_cast<long double>(point.x) < intersection_x)
                    {
                        inside = !inside;
                    }
                }
            }

            return inside;
        }

        [[nodiscard]] std::span<PointI const> ValidatePath(std::span<PointI const> points)
        {
            if (points.size() >= 2 && points.front() == points.back())
            {
                points = points.first(points.size() - 1);
            }
            if (points.size() < 3)
            {
                throw std::invalid_argument("A closed selection path must contain at least three vertices.");
            }

            long double twice_area = 0.0L;
            for (std::size_t index = 0; index < points.size(); ++index)
            {
                auto const next = (index + 1) % points.size();
                twice_area += static_cast<long double>(points[index].x) * points[next].y
                    - static_cast<long double>(points[next].x) * points[index].y;
            }
            if (twice_area == 0.0L)
            {
                throw std::invalid_argument("A closed selection path must enclose a non-zero area.");
            }

            return points;
        }

        [[nodiscard]] SelectionMask RasterizeClosedPath(RectI const canvas_bounds, std::span<PointI const> points)
        {
            ValidateBounds(canvas_bounds);
            points = ValidatePath(points);

            auto min_x = points.front().x;
            auto max_x = points.front().x;
            auto min_y = points.front().y;
            auto max_y = points.front().y;
            for (auto const point : points.subspan(1))
            {
                min_x = std::min(min_x, point.x);
                max_x = std::max(max_x, point.x);
                min_y = std::min(min_y, point.y);
                max_y = std::max(max_y, point.y);
            }

            auto const width = static_cast<std::int64_t>(max_x) - min_x + 1;
            auto const height = static_cast<std::int64_t>(max_y) - min_y + 1;
            if (width > std::numeric_limits<std::int32_t>::max() || height > std::numeric_limits<std::int32_t>::max())
            {
                throw std::overflow_error("Selection path bounds exceed the supported integer rectangle range.");
            }

            auto const path_bounds = RectI{ min_x, min_y, static_cast<std::int32_t>(width), static_cast<std::int32_t>(height) };
            auto const clipped = Intersect(canvas_bounds, path_bounds);
            return MakeMask(clipped, [points](PointI const point) { return PointInClosedPath(point, points); });
        }

        [[nodiscard]] std::int32_t FloorTileCoordinate(std::int32_t const coordinate) noexcept
        {
            auto quotient = coordinate / static_cast<std::int32_t>(TileExtent);
            auto const remainder = coordinate % static_cast<std::int32_t>(TileExtent);
            if (remainder < 0)
            {
                --quotient;
            }
            return quotient;
        }

        [[nodiscard]] std::uint32_t LocalCoordinate(std::int32_t const coordinate, std::int32_t const tile) noexcept
        {
            return static_cast<std::uint32_t>(
                static_cast<std::int64_t>(coordinate) - static_cast<std::int64_t>(tile) * TileExtent);
        }

        [[nodiscard]] std::uint8_t CompositeChannel(
            std::uint8_t const source,
            std::uint8_t const destination,
            std::uint8_t const source_alpha) noexcept
        {
            auto const retained = (static_cast<std::uint32_t>(destination) * (255U - source_alpha) + 127U) / 255U;
            return static_cast<std::uint8_t>(std::min<std::uint32_t>(255U, static_cast<std::uint32_t>(source) + retained));
        }

        [[nodiscard]] std::uint8_t MixPreservingAlpha(
            std::uint8_t const source,
            std::uint8_t const destination,
            std::uint8_t const mix) noexcept
        {
            auto const result = static_cast<std::uint32_t>(source) * mix
                + static_cast<std::uint32_t>(destination) * (255U - mix);
            return static_cast<std::uint8_t>((result + 127U) / 255U);
        }
    }

    bool RectI::IsValid() const noexcept
    {
        if (width <= 0 || height <= 0)
        {
            return false;
        }

        auto const right = static_cast<std::int64_t>(x) + width;
        auto const bottom = static_cast<std::int64_t>(y) + height;
        return right <= static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()) + 1
            && bottom <= static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()) + 1;
    }

    bool RectI::Contains(PointI const point) const noexcept
    {
        return IsValid()
            && static_cast<std::int64_t>(point.x) >= x
            && static_cast<std::int64_t>(point.y) >= y
            && static_cast<std::int64_t>(point.x) < static_cast<std::int64_t>(x) + width
            && static_cast<std::int64_t>(point.y) < static_cast<std::int64_t>(y) + height;
    }

    PressureMapper::PressureMapper(PressureSensitivity const sensitivity)
        : sensitivity_(sensitivity)
    {
        if (!std::isfinite(sensitivity.minimum_input) || !std::isfinite(sensitivity.maximum_input)
            || sensitivity.minimum_input < 0.0F || sensitivity.maximum_input > 1.0F
            || sensitivity.minimum_input >= sensitivity.maximum_input)
        {
            throw std::invalid_argument("Pressure sensitivity input bounds must be finite, ordered, and within zero to one.");
        }
        if (!std::isfinite(sensitivity.gamma) || sensitivity.gamma <= 0.0F || sensitivity.gamma > 100.0F)
        {
            throw std::invalid_argument("Pressure sensitivity gamma must be finite, greater than zero, and at most one hundred.");
        }
    }

    PressureSensitivity PressureMapper::Sensitivity() const noexcept
    {
        return sensitivity_;
    }

    float PressureMapper::Map(float const raw_pressure) const
    {
        if (!std::isfinite(raw_pressure))
        {
            throw std::invalid_argument("Raw stylus pressure must be finite.");
        }

        auto const normalized = std::clamp(
            (raw_pressure - sensitivity_.minimum_input)
                / (sensitivity_.maximum_input - sensitivity_.minimum_input),
            0.0F,
            1.0F);
        return std::pow(normalized, sensitivity_.gamma);
    }

    StrokeStabilizer::StrokeStabilizer(StrokeStabilizerSettings const settings)
        : settings_(settings)
    {
        if (!std::isfinite(settings.strength) || settings.strength < 0.0F || settings.strength > 1.0F)
        {
            throw std::invalid_argument("Stroke stabilizer strength must be finite and between zero and one.");
        }
    }

    StrokeStabilizerSettings StrokeStabilizer::Settings() const noexcept
    {
        return settings_;
    }

    StylusSample StrokeStabilizer::Push(StylusSample const sample)
    {
        if (!std::isfinite(sample.pressure) || sample.pressure < 0.0F || sample.pressure > 1.0F)
        {
            throw std::invalid_argument("Stylus sample pressure must be finite and between zero and one.");
        }

        last_raw_ = sample;
        if (!has_sample_ || !settings_.enabled || settings_.strength == 0.0F)
        {
            smoothed_x_ = sample.position.x;
            smoothed_y_ = sample.position.y;
            smoothed_pressure_ = sample.pressure;
            last_output_ = sample;
            has_sample_ = true;
            return sample;
        }

        auto const input_weight = 1.0 - static_cast<double>(settings_.strength);
        smoothed_x_ += (static_cast<double>(sample.position.x) - smoothed_x_) * input_weight;
        smoothed_y_ += (static_cast<double>(sample.position.y) - smoothed_y_) * input_weight;
        smoothed_pressure_ += (static_cast<double>(sample.pressure) - smoothed_pressure_) * input_weight;
        last_output_ = {
            { static_cast<std::int32_t>(std::llround(smoothed_x_)), static_cast<std::int32_t>(std::llround(smoothed_y_)) },
            static_cast<float>(std::clamp(smoothed_pressure_, 0.0, 1.0))
        };
        return last_output_;
    }

    std::optional<StylusSample> StrokeStabilizer::Flush() noexcept
    {
        if (!has_sample_)
        {
            return std::nullopt;
        }

        auto const endpoint_needed = last_output_.position != last_raw_.position
            || last_output_.pressure != last_raw_.pressure;
        auto endpoint = endpoint_needed ? std::optional<StylusSample>(last_raw_) : std::nullopt;
        Reset();
        return endpoint;
    }

    void StrokeStabilizer::Reset() noexcept
    {
        smoothed_x_ = 0.0;
        smoothed_y_ = 0.0;
        smoothed_pressure_ = 0.0;
        last_raw_ = {};
        last_output_ = {};
        has_sample_ = false;
    }

    ColorState::ColorState() noexcept
    {
        Reset();
    }

    ColorState::ColorState(Rgba8 const foreground, Rgba8 const background) noexcept
        : foreground_(foreground), background_(background)
    {
    }

    Rgba8 ColorState::Foreground() const noexcept { return foreground_; }
    Rgba8 ColorState::Background() const noexcept { return background_; }
    void ColorState::SetForeground(Rgba8 const color) noexcept { foreground_ = color; }
    void ColorState::SetBackground(Rgba8 const color) noexcept { background_ = color; }

    void ColorState::Swap() noexcept
    {
        std::swap(foreground_, background_);
    }

    void ColorState::Reset() noexcept
    {
        foreground_ = { 0, 0, 0, 255 };
        background_ = { 255, 255, 255, 255 };
    }

    std::vector<PaintPixel> RasterizePencilLine(PointI const start, PointI const end, Rgba8 const color)
    {
        auto const delta_x = std::abs(static_cast<std::int64_t>(end.x) - start.x);
        auto const delta_y = std::abs(static_cast<std::int64_t>(end.y) - start.y);
        auto const count = std::max(delta_x, delta_y) + 1;
        if (count > static_cast<std::int64_t>(MaxGeneratedElements))
        {
            throw std::length_error("A pencil segment cannot exceed the deterministic pixel limit.");
        }

        std::vector<PaintPixel> pixels;
        pixels.reserve(static_cast<std::size_t>(count));
        auto x = static_cast<std::int64_t>(start.x);
        auto y = static_cast<std::int64_t>(start.y);
        auto const target_x = static_cast<std::int64_t>(end.x);
        auto const target_y = static_cast<std::int64_t>(end.y);
        auto const step_x = x < target_x ? 1 : -1;
        auto const step_y = y < target_y ? 1 : -1;
        auto error = delta_x - delta_y;

        for (;;)
        {
            pixels.push_back({ { static_cast<std::int32_t>(x), static_cast<std::int32_t>(y) }, color, 1, 1.0F });
            if (x == target_x && y == target_y)
            {
                break;
            }

            auto const twice_error = error * 2;
            if (twice_error > -delta_y)
            {
                error -= delta_y;
                x += step_x;
            }
            if (twice_error < delta_x)
            {
                error += delta_x;
                y += step_y;
            }
        }

        return pixels;
    }

    std::vector<PaintPixel> RasterizePencilSamples(
        StylusSample const start,
        StylusSample const end,
        Rgba8 const color)
    {
        if (!std::isfinite(start.pressure) || start.pressure < 0.0F || start.pressure > 1.0F
            || !std::isfinite(end.pressure) || end.pressure < 0.0F || end.pressure > 1.0F)
        {
            throw std::invalid_argument("Pencil stylus pressure must be finite and between zero and one.");
        }
        return RasterizePencilLine(start.position, end.position, color);
    }

    AirbrushAccumulator::AirbrushAccumulator(AirbrushSettings const settings)
        : settings_(settings)
    {
        auto const maximum_radius = (std::sqrt(static_cast<double>(MaxGeneratedElements)) - 1.0) / 2.0;
        if (!std::isfinite(settings.radius) || settings.radius < 0.01F
            || static_cast<double>(settings.radius) > maximum_radius)
        {
            throw std::invalid_argument("Airbrush radius must be finite, at least 0.01 pixel, and within the deterministic pixel limit.");
        }
        if (!std::isfinite(settings.flow_per_second)
            || settings.flow_per_second < 0.0F || settings.flow_per_second > 1.0F)
        {
            throw std::invalid_argument("Airbrush flow must be finite and between zero and one.");
        }
        if (!std::isfinite(settings.fixed_timestep_seconds) || settings.fixed_timestep_seconds <= 0.0F)
        {
            throw std::invalid_argument("Airbrush fixed timestep must be finite and greater than zero.");
        }
        if (!std::isfinite(settings.hardness) || settings.hardness < 0.0F || settings.hardness > 1.0F)
        {
            throw std::invalid_argument("Brush hardness must be finite and between zero and one.");
        }
        if (!std::isfinite(settings.spacing) || settings.spacing < 0.01F || settings.spacing > 10.0F)
        {
            throw std::invalid_argument("Brush spacing must be a finite diameter ratio from 0.01 through ten.");
        }
        if (!std::isfinite(settings.opacity) || settings.opacity < 0.0F || settings.opacity > 1.0F)
        {
            throw std::invalid_argument("Brush opacity must be finite and between zero and one.");
        }
    }

    AirbrushSettings AirbrushAccumulator::Settings() const noexcept { return settings_; }
    double AirbrushAccumulator::PendingSeconds() const noexcept { return pending_seconds_; }

    std::vector<PaintDab> AirbrushAccumulator::Advance(
        PointI const from,
        PointI const to,
        double const elapsed_seconds,
        float const pressure,
        Rgba8 const color)
    {
        if (!std::isfinite(elapsed_seconds) || elapsed_seconds < 0.0)
        {
            throw std::invalid_argument("Airbrush elapsed time must be finite and non-negative.");
        }
        if (!std::isfinite(pressure) || pressure < 0.0F || pressure > 1.0F)
        {
            throw std::invalid_argument("Airbrush pressure must be finite and between zero and one.");
        }
        auto const timestep = static_cast<double>(settings_.fixed_timestep_seconds);
        auto const total = pending_seconds_ + elapsed_seconds;
        // Settings arrive as floats, so tolerate their representation error while
        // keeping the fixed-step schedule stable across equivalent event chunks.
        auto const epsilon = timestep * 1.0e-6;
        auto const time_count = static_cast<std::uint64_t>(std::floor((total + epsilon) / timestep));
        if (time_count > MaxGeneratedElements)
        {
            throw std::length_error("An airbrush advance cannot exceed the deterministic dab limit.");
        }

        std::vector<double> candidate_fractions;
        candidate_fractions.reserve(static_cast<std::size_t>(time_count) + 1);
        if (elapsed_seconds > 0.0)
        {
            auto first_time = timestep - pending_seconds_;
            if (first_time < 0.0)
            {
                first_time = 0.0;
            }
            for (std::uint64_t index = 0; index < time_count; ++index)
            {
                auto const at = std::min(elapsed_seconds, first_time + static_cast<double>(index) * timestep);
                candidate_fractions.push_back(at / elapsed_seconds);
            }
        }

        auto const pressure_size = settings_.pressure_affects_size ? pressure : 1.0F;
        auto const radius = settings_.radius * pressure_size;
        auto const spacing_distance = static_cast<double>(radius) * 2.0 * settings_.spacing;
        auto const delta_x = static_cast<double>(static_cast<std::int64_t>(to.x) - from.x);
        auto const delta_y = static_cast<double>(static_cast<std::int64_t>(to.y) - from.y);
        auto const segment_length = std::hypot(delta_x, delta_y);

        if (!has_last_position_ || last_position_ != from)
        {
            distance_until_next_dab_ = 0.0;
        }
        if (radius > 0.0F)
        {
            auto distance = distance_until_next_dab_;
            auto const distance_epsilon = std::max(1.0, spacing_distance) * 1.0e-9;
            if (segment_length == 0.0)
            {
                if (distance <= distance_epsilon)
                {
                    candidate_fractions.push_back(0.0);
                    distance_until_next_dab_ = spacing_distance;
                }
            }
            else
            {
                while (distance <= segment_length + distance_epsilon)
                {
                    candidate_fractions.push_back(std::clamp(distance / segment_length, 0.0, 1.0));
                    if (candidate_fractions.size() > MaxGeneratedElements)
                    {
                        throw std::length_error("An airbrush advance cannot exceed the deterministic dab limit.");
                    }
                    distance += spacing_distance;
                }
                distance_until_next_dab_ = std::max(0.0, distance - segment_length);
            }
        }
        else
        {
            distance_until_next_dab_ = 0.0;
        }

        last_position_ = to;
        has_last_position_ = true;

        std::sort(candidate_fractions.begin(), candidate_fractions.end());
        candidate_fractions.erase(
            std::unique(candidate_fractions.begin(), candidate_fractions.end(), [](double const left, double const right)
            {
                return std::abs(left - right) <= 1.0e-9;
            }),
            candidate_fractions.end());

        auto const pressure_opacity = settings_.pressure_affects_opacity ? pressure : 1.0F;
        auto const opacity = std::clamp(
            settings_.flow_per_second * settings_.fixed_timestep_seconds * settings_.opacity * pressure_opacity,
            0.0F,
            1.0F);

        std::vector<PaintDab> dabs;
        if (radius > 0.0F && opacity > 0.0F)
        {
            dabs.reserve(candidate_fractions.size());
            for (auto const fraction : candidate_fractions)
            {
                auto const x = static_cast<double>(from.x) + delta_x * fraction;
                auto const y = static_cast<double>(from.y) + delta_y * fraction;
                dabs.push_back({
                    { static_cast<std::int32_t>(std::llround(x)), static_cast<std::int32_t>(std::llround(y)) },
                    radius,
                    color,
                    opacity,
                    settings_.hardness
                });
            }
        }

        pending_seconds_ = total - static_cast<double>(time_count) * timestep;
        if (pending_seconds_ < epsilon || timestep - pending_seconds_ < epsilon)
        {
            pending_seconds_ = 0.0;
        }
        return dabs;
    }

    std::vector<PaintDab> AirbrushAccumulator::Advance(
        StylusSample const from,
        StylusSample const to,
        double const elapsed_seconds,
        Rgba8 const color)
    {
        return Advance(from.position, to.position, elapsed_seconds, to.pressure, color);
    }

    void AirbrushAccumulator::Reset() noexcept
    {
        pending_seconds_ = 0.0;
        distance_until_next_dab_ = 0.0;
        last_position_ = {};
        has_last_position_ = false;
    }

    std::vector<PaintPixel> RasterizeDabs(std::span<PaintDab const> const dabs)
    {
        std::vector<PaintPixel> pixels;
        for (auto const& dab : dabs)
        {
            if (!std::isfinite(dab.radius) || dab.radius <= 0.0F
                || !std::isfinite(dab.opacity) || dab.opacity < 0.0F || dab.opacity > 1.0F
                || !std::isfinite(dab.hardness) || dab.hardness < 0.0F || dab.hardness > 1.0F)
            {
                throw std::invalid_argument("Paint dab radius, opacity, or hardness is outside its valid range.");
            }

            auto const maximum_radius = (std::sqrt(static_cast<double>(MaxGeneratedElements)) - 1.0) / 2.0;
            if (static_cast<double>(dab.radius) > maximum_radius)
            {
                throw std::length_error("A paint dab cannot exceed the deterministic pixel limit.");
            }

            auto const extent = static_cast<std::int64_t>(std::ceil(dab.radius));
            auto const diameter = extent * 2 + 1;
            if (diameter * diameter > static_cast<std::int64_t>(MaxGeneratedElements)
                || pixels.size() + static_cast<std::size_t>(diameter * diameter) > MaxGeneratedElements)
            {
                throw std::length_error("Rasterized dabs cannot exceed the deterministic pixel limit.");
            }

            auto const radius_squared = static_cast<double>(dab.radius) * dab.radius;
            for (std::int64_t offset_y = -extent; offset_y <= extent; ++offset_y)
            {
                for (std::int64_t offset_x = -extent; offset_x <= extent; ++offset_x)
                {
                    if (static_cast<double>(offset_x * offset_x + offset_y * offset_y) > radius_squared)
                    {
                        continue;
                    }

                    auto const x = static_cast<std::int64_t>(dab.center.x) + offset_x;
                    auto const y = static_cast<std::int64_t>(dab.center.y) + offset_y;
                    if (x < std::numeric_limits<std::int32_t>::min() || x > std::numeric_limits<std::int32_t>::max()
                        || y < std::numeric_limits<std::int32_t>::min() || y > std::numeric_limits<std::int32_t>::max())
                    {
                        continue;
                    }
                    auto const normalized_distance = std::sqrt(
                        static_cast<double>(offset_x * offset_x + offset_y * offset_y)) / dab.radius;
                    auto pixel_opacity = dab.opacity;
                    if (normalized_distance > dab.hardness && dab.hardness < 1.0F)
                    {
                        auto const falloff = std::clamp(
                            (1.0 - normalized_distance) / (1.0 - static_cast<double>(dab.hardness)),
                            0.0,
                            1.0);
                        pixel_opacity *= static_cast<float>(falloff);
                    }
                    pixels.push_back({
                        { static_cast<std::int32_t>(x), static_cast<std::int32_t>(y) },
                        dab.color,
                        1,
                        pixel_opacity
                    });
                }
            }
        }

        return pixels;
    }

    void ApplyPaintPixels(
        SparseTileStore& tiles,
        std::span<PaintPixel const> const pixels,
        PaintApplicationOptions const options)
    {
        std::map<TileKey, std::vector<PaintPixel const*>> by_tile;
        for (auto const& pixel : pixels)
        {
            if (pixel.coverage > 1)
            {
                throw std::invalid_argument("Paint pixel coverage must be binary zero or one.");
            }
            if (!std::isfinite(pixel.opacity) || pixel.opacity < 0.0F || pixel.opacity > 1.0F)
            {
                throw std::invalid_argument("Paint pixel opacity must be finite and between zero and one.");
            }
            if (pixel.coverage == 0 || pixel.opacity == 0.0F || pixel.color.alpha == 0)
            {
                continue;
            }

            by_tile[{ FloorTileCoordinate(pixel.position.x), FloorTileCoordinate(pixel.position.y), 0 }].push_back(&pixel);
        }

        for (auto const& [key, tile_pixels] : by_tile)
        {
            auto draft = tiles.BeginWrite(key);
            auto bytes = draft.Pixels();
            for (auto const* const pixel : tile_pixels)
            {
                auto const local_x = LocalCoordinate(pixel->position.x, key.x);
                auto const local_y = LocalCoordinate(pixel->position.y, key.y);
                auto const byte_index = (static_cast<std::size_t>(local_y) * TileExtent + local_x) * Rgba8BytesPerPixel;
                auto const effective_alpha = static_cast<std::uint8_t>(std::lround(
                    static_cast<double>(pixel->color.alpha) * pixel->opacity * pixel->coverage));

                auto const destination_alpha = std::to_integer<std::uint8_t>(bytes[byte_index + 3]);
                if (options.alpha_locked)
                {
                    if (destination_alpha == 0)
                    {
                        continue;
                    }

                    auto const locked_red = static_cast<std::uint8_t>(
                        (static_cast<std::uint32_t>(pixel->color.red) * destination_alpha + 127U) / 255U);
                    auto const locked_green = static_cast<std::uint8_t>(
                        (static_cast<std::uint32_t>(pixel->color.green) * destination_alpha + 127U) / 255U);
                    auto const locked_blue = static_cast<std::uint8_t>(
                        (static_cast<std::uint32_t>(pixel->color.blue) * destination_alpha + 127U) / 255U);
                    bytes[byte_index] = static_cast<std::byte>(MixPreservingAlpha(
                        locked_red, std::to_integer<std::uint8_t>(bytes[byte_index]), effective_alpha));
                    bytes[byte_index + 1] = static_cast<std::byte>(MixPreservingAlpha(
                        locked_green, std::to_integer<std::uint8_t>(bytes[byte_index + 1]), effective_alpha));
                    bytes[byte_index + 2] = static_cast<std::byte>(MixPreservingAlpha(
                        locked_blue, std::to_integer<std::uint8_t>(bytes[byte_index + 2]), effective_alpha));
                    continue;
                }

                auto const source_red = static_cast<std::uint8_t>((static_cast<std::uint32_t>(pixel->color.red) * effective_alpha + 127U) / 255U);
                auto const source_green = static_cast<std::uint8_t>((static_cast<std::uint32_t>(pixel->color.green) * effective_alpha + 127U) / 255U);
                auto const source_blue = static_cast<std::uint8_t>((static_cast<std::uint32_t>(pixel->color.blue) * effective_alpha + 127U) / 255U);

                bytes[byte_index] = static_cast<std::byte>(CompositeChannel(source_red, std::to_integer<std::uint8_t>(bytes[byte_index]), effective_alpha));
                bytes[byte_index + 1] = static_cast<std::byte>(CompositeChannel(source_green, std::to_integer<std::uint8_t>(bytes[byte_index + 1]), effective_alpha));
                bytes[byte_index + 2] = static_cast<std::byte>(CompositeChannel(source_blue, std::to_integer<std::uint8_t>(bytes[byte_index + 2]), effective_alpha));
                bytes[byte_index + 3] = static_cast<std::byte>(CompositeChannel(effective_alpha, std::to_integer<std::uint8_t>(bytes[byte_index + 3]), effective_alpha));
            }
            static_cast<void>(tiles.Publish(key, std::move(draft)));
        }
    }

    SelectionMask::SelectionMask(RectI const bounds, std::vector<std::uint8_t> coverage) noexcept
        : bounds_(bounds), coverage_(std::move(coverage))
    {
    }

    RectI SelectionMask::Bounds() const noexcept { return bounds_; }
    bool SelectionMask::Empty() const noexcept { return coverage_.empty(); }

    std::uint8_t SelectionMask::CoverageAt(PointI const point) const noexcept
    {
        return bounds_.Contains(point) ? coverage_[PixelIndex(bounds_, point)] : 0U;
    }

    std::span<std::uint8_t const> SelectionMask::Coverage() const noexcept { return coverage_; }

    SelectionMask RasterizeRectangularSelection(RectI const canvas_bounds, RectI const selection_bounds)
    {
        ValidateBounds(canvas_bounds);
        ValidateBounds(selection_bounds);
        auto const clipped = Intersect(canvas_bounds, selection_bounds);
        return MakeMask(clipped, [](PointI) { return true; });
    }

    SelectionMask RasterizeEllipticalSelection(RectI const canvas_bounds, RectI const selection_bounds)
    {
        ValidateBounds(canvas_bounds);
        ValidateBounds(selection_bounds);
        auto const clipped = Intersect(canvas_bounds, selection_bounds);
        auto const center_x = static_cast<long double>(selection_bounds.x) + (selection_bounds.width - 1) / 2.0L;
        auto const center_y = static_cast<long double>(selection_bounds.y) + (selection_bounds.height - 1) / 2.0L;
        auto const radius_x = (selection_bounds.width - 1) / 2.0L;
        auto const radius_y = (selection_bounds.height - 1) / 2.0L;
        return MakeMask(clipped, [=](PointI const point)
        {
            auto const normalized_x = radius_x == 0.0L ? (point.x == selection_bounds.x ? 0.0L : 2.0L)
                : (point.x - center_x) / radius_x;
            auto const normalized_y = radius_y == 0.0L ? (point.y == selection_bounds.y ? 0.0L : 2.0L)
                : (point.y - center_y) / radius_y;
            return normalized_x * normalized_x + normalized_y * normalized_y <= 1.0L;
        });
    }

    SelectionMask RasterizeFreehandSelection(RectI const canvas_bounds, std::span<PointI const> const closed_path)
    {
        return RasterizeClosedPath(canvas_bounds, closed_path);
    }

    SelectionMask RasterizePolygonalSelection(RectI const canvas_bounds, std::span<PointI const> const vertices)
    {
        return RasterizeClosedPath(canvas_bounds, vertices);
    }
}
