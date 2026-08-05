#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <octopaint/core/Tile.h>

namespace octopaint::core
{
    enum class ToolKind : std::uint8_t
    {
        Pencil,
        Airbrush,
        RectangularMarquee,
        EllipticalMarquee,
        FreehandLasso,
        PolygonalLasso,
        MoveLayer
    };

    struct PointI final
    {
        std::int32_t x{};
        std::int32_t y{};

        auto operator<=>(PointI const&) const = default;
    };

    struct RectI final
    {
        std::int32_t x{};
        std::int32_t y{};
        std::int32_t width{};
        std::int32_t height{};

        [[nodiscard]] bool IsValid() const noexcept;
        [[nodiscard]] bool Contains(PointI point) const noexcept;

        auto operator<=>(RectI const&) const = default;
    };

    struct StylusSample final
    {
        PointI position;
        float pressure{ 1.0F };

        bool operator==(StylusSample const&) const = default;
    };

    struct PressureSensitivity final
    {
        float minimum_input{ 0.0F };
        float maximum_input{ 1.0F };
        float gamma{ 1.0F };
    };

    class PressureMapper final
    {
    public:
        explicit PressureMapper(PressureSensitivity sensitivity);

        [[nodiscard]] PressureSensitivity Sensitivity() const noexcept;
        [[nodiscard]] float Map(float raw_pressure) const;

    private:
        PressureSensitivity sensitivity_;
    };

    struct StrokeStabilizerSettings final
    {
        bool enabled{ false };
        // 0 is exact passthrough. 1 holds at the initial sample until Flush.
        // Intermediate values use deterministic exponential smoothing.
        float strength{ 0.5F };
    };

    class StrokeStabilizer final
    {
    public:
        explicit StrokeStabilizer(StrokeStabilizerSettings settings);

        [[nodiscard]] StrokeStabilizerSettings Settings() const noexcept;
        [[nodiscard]] StylusSample Push(StylusSample sample);
        [[nodiscard]] std::optional<StylusSample> Flush() noexcept;
        void Reset() noexcept;

    private:
        StrokeStabilizerSettings settings_;
        double smoothed_x_{};
        double smoothed_y_{};
        double smoothed_pressure_{};
        StylusSample last_raw_{};
        StylusSample last_output_{};
        bool has_sample_{};
    };

    struct Rgba8 final
    {
        std::uint8_t red{};
        std::uint8_t green{};
        std::uint8_t blue{};
        std::uint8_t alpha{ 255 };

        auto operator<=>(Rgba8 const&) const = default;
    };

    class ColorState final
    {
    public:
        ColorState() noexcept;
        ColorState(Rgba8 foreground, Rgba8 background) noexcept;

        [[nodiscard]] Rgba8 Foreground() const noexcept;
        [[nodiscard]] Rgba8 Background() const noexcept;
        void SetForeground(Rgba8 color) noexcept;
        void SetBackground(Rgba8 color) noexcept;
        void Swap() noexcept;
        void Reset() noexcept;

    private:
        Rgba8 foreground_;
        Rgba8 background_;
    };

    struct PaintPixel final
    {
        PointI position;
        Rgba8 color;
        std::uint8_t coverage{ 1 }; // Binary geometric coverage: 0 or 1.
        float opacity{ 1.0F };
    };

    [[nodiscard]] std::vector<PaintPixel> RasterizePencilLine(
        PointI start,
        PointI end,
        Rgba8 color);
    [[nodiscard]] std::vector<PaintPixel> RasterizePencilSamples(
        StylusSample start,
        StylusSample end,
        Rgba8 color);

    struct BrushSettings final
    {
        // Radius is measured in pixels. Spacing is a ratio of the current
        // pressure-adjusted diameter (for example, 0.25 means 25% of diameter).
        float radius{ 8.0F };
        float flow_per_second{ 1.0F };
        float fixed_timestep_seconds{ 1.0F / 60.0F };
        float hardness{ 1.0F };
        float spacing{ 0.25F };
        float opacity{ 1.0F };
        bool pressure_affects_size{ false };
        bool pressure_affects_opacity{ true };
    };

    using AirbrushSettings = BrushSettings;

    struct PaintDab final
    {
        PointI center;
        float radius{};
        Rgba8 color;
        float opacity{};
        float hardness{ 1.0F };
    };

    class AirbrushAccumulator final
    {
    public:
        explicit AirbrushAccumulator(AirbrushSettings settings);

        [[nodiscard]] AirbrushSettings Settings() const noexcept;
        [[nodiscard]] double PendingSeconds() const noexcept;
        [[nodiscard]] std::vector<PaintDab> Advance(
            PointI from,
            PointI to,
            double elapsed_seconds,
            float pressure,
            Rgba8 color);
        [[nodiscard]] std::vector<PaintDab> Advance(
            StylusSample from,
            StylusSample to,
            double elapsed_seconds,
            Rgba8 color);
        void Reset() noexcept;

    private:
        AirbrushSettings settings_;
        double pending_seconds_{};
        double distance_until_next_dab_{};
        PointI last_position_{};
        bool has_last_position_{};
    };

    [[nodiscard]] std::vector<PaintPixel> RasterizeDabs(std::span<PaintDab const> dabs);

    struct PaintApplicationOptions final
    {
        // Alpha lock skips fully transparent destinations and preserves every
        // existing destination alpha value while changing premultiplied color.
        bool alpha_locked{ false };
    };

    void ApplyPaintPixels(
        SparseTileStore& tiles,
        std::span<PaintPixel const> pixels,
        PaintApplicationOptions options = {});

    class SelectionMask final
    {
    public:
        SelectionMask() = default;

        [[nodiscard]] RectI Bounds() const noexcept;
        [[nodiscard]] bool Empty() const noexcept;
        [[nodiscard]] std::uint8_t CoverageAt(PointI point) const noexcept;
        [[nodiscard]] std::span<std::uint8_t const> Coverage() const noexcept;

    private:
        friend struct SelectionMaskAccess;
        friend SelectionMask RasterizeRectangularSelection(RectI, RectI);
        friend SelectionMask RasterizeEllipticalSelection(RectI, RectI);
        friend SelectionMask RasterizeFreehandSelection(RectI, std::span<PointI const>);
        friend SelectionMask RasterizePolygonalSelection(RectI, std::span<PointI const>);

        SelectionMask(RectI bounds, std::vector<std::uint8_t> coverage) noexcept;

        RectI bounds_;
        std::vector<std::uint8_t> coverage_;
    };

    [[nodiscard]] SelectionMask RasterizeRectangularSelection(RectI canvas_bounds, RectI selection_bounds);
    [[nodiscard]] SelectionMask RasterizeEllipticalSelection(RectI canvas_bounds, RectI selection_bounds);
    [[nodiscard]] SelectionMask RasterizeFreehandSelection(RectI canvas_bounds, std::span<PointI const> closed_path);
    [[nodiscard]] SelectionMask RasterizePolygonalSelection(RectI canvas_bounds, std::span<PointI const> vertices);
}
