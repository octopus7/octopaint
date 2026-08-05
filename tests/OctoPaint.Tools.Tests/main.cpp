#include <octopaint/core/Tools.h>

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
    using namespace octopaint::core;

    void Require(bool const condition, char const* const message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    template<typename Function>
    void RequireInvalidArgument(Function&& function, char const* const message)
    {
        try
        {
            std::forward<Function>(function)();
        }
        catch (std::invalid_argument const&)
        {
            return;
        }

        throw std::runtime_error(message);
    }

    [[nodiscard]] std::uint8_t TileByte(
        SparseTileStore const& store,
        TileKey const key,
        std::uint32_t const local_x,
        std::uint32_t const local_y,
        std::uint32_t const channel)
    {
        auto const tile = store.Read(key);
        Require(tile.has_value(), "The expected paint tile was not allocated.");
        auto const index = (static_cast<std::size_t>(local_y) * TileExtent + local_x) * Rgba8BytesPerPixel + channel;
        return std::to_integer<std::uint8_t>(tile->Pixels()[index]);
    }

    void TestToolAndColorContracts()
    {
        Require(ToolKind::Pencil != ToolKind::Airbrush && ToolKind::PolygonalLasso != ToolKind::MoveLayer,
            "Every requested tool kind must have a distinct contract value.");

        ColorState colors;
        Require(colors.Foreground() == Rgba8{ 0, 0, 0, 255 }, "Default foreground must be opaque black.");
        Require(colors.Background() == Rgba8{ 255, 255, 255, 255 }, "Default background must be opaque white.");
        colors.SetForeground({ 10, 20, 30, 40 });
        colors.SetBackground({ 50, 60, 70, 80 });
        colors.Swap();
        Require(colors.Foreground() == Rgba8{ 50, 60, 70, 80 }
            && colors.Background() == Rgba8{ 10, 20, 30, 40 },
            "Color swap must exchange all RGBA channels.");
        colors.Reset();
        Require(colors.Foreground() == Rgba8{ 0, 0, 0, 255 }
            && colors.Background() == Rgba8{ 255, 255, 255, 255 },
            "Color reset must restore canonical black and white.");
    }

    void TestPencilBresenhamAndTileApplication()
    {
        auto const pixels = RasterizePencilLine({ 0, 0 }, { 100, 7 }, { 25, 50, 75, 255 });
        Require(pixels.size() == 101, "A fast shallow pencil move must emit every major-axis integer pixel.");
        for (std::size_t index = 0; index < pixels.size(); ++index)
        {
            Require(pixels[index].position.x == static_cast<std::int32_t>(index),
                "A fast pencil move must have no major-axis pixel gaps.");
            Require(pixels[index].coverage == 1 && pixels[index].opacity == 1.0F,
                "Pencil output must be binary and non-antialiased.");
            if (index != 0)
            {
                auto const dx = std::abs(pixels[index].position.x - pixels[index - 1].position.x);
                auto const dy = std::abs(pixels[index].position.y - pixels[index - 1].position.y);
                Require(dx <= 1 && dy <= 1, "Adjacent Bresenham pixels must remain 8-connected.");
            }
        }

        auto const reverse = RasterizePencilLine({ 100, 7 }, { 0, 0 }, { 1, 2, 3, 255 });
        Require(reverse.size() == pixels.size() && reverse.front().position == pixels.back().position
            && reverse.back().position == pixels.front().position,
            "Pencil rasterization must include both endpoints in either direction.");

        auto crossing = RasterizePencilLine({ -1, 0 }, { 1, 0 }, { 255, 0, 0, 255 });
        SparseTileStore store;
        ApplyPaintPixels(store, crossing);
        Require(store.TileCount() == 2, "Paint application must map negative and non-negative pixels to adjacent sparse tiles.");
        Require(TileByte(store, { -1, 0, 0 }, 255, 0, 0) == 255
            && TileByte(store, { -1, 0, 0 }, 255, 0, 3) == 255,
            "A negative-coordinate pencil pixel must be written as premultiplied RGBA.");
        Require(TileByte(store, { 0, 0, 0 }, 0, 0, 0) == 255
            && TileByte(store, { 0, 0, 0 }, 1, 0, 3) == 255,
            "Positive pixels must be written to the expected local tile coordinates.");

        std::vector<PaintPixel> invalid{
            { .position = { 2, 2 }, .color = { 1, 2, 3, 255 }, .coverage = 2, .opacity = 1.0F }
        };
        RequireInvalidArgument([&] { ApplyPaintPixels(store, invalid); },
            "The tile application boundary must reject antialiased/non-binary coverage values.");

        SparseTileStore alpha_locked_store;
        std::vector<PaintPixel> const seed{
            { .position = { 4, 4 }, .color = { 255, 0, 0, 128 }, .coverage = 1, .opacity = 1.0F }
        };
        ApplyPaintPixels(alpha_locked_store, seed);
        std::vector<PaintPixel> const locked_paint{
            { .position = { 4, 4 }, .color = { 0, 0, 255, 255 }, .coverage = 1, .opacity = 1.0F },
            { .position = { 5, 4 }, .color = { 0, 0, 255, 255 }, .coverage = 1, .opacity = 1.0F }
        };
        ApplyPaintPixels(alpha_locked_store, locked_paint, { .alpha_locked = true });
        Require(TileByte(alpha_locked_store, { 0, 0, 0 }, 4, 4, 3) == 128,
            "Alpha-locked painting must preserve an existing partial alpha value.");
        Require(TileByte(alpha_locked_store, { 0, 0, 0 }, 4, 4, 0) == 0
            && TileByte(alpha_locked_store, { 0, 0, 0 }, 4, 4, 2) == 128,
            "Alpha-locked painting must recolor within the existing premultiplied alpha envelope.");
        Require(TileByte(alpha_locked_store, { 0, 0, 0 }, 5, 4, 0) == 0
            && TileByte(alpha_locked_store, { 0, 0, 0 }, 5, 4, 2) == 0
            && TileByte(alpha_locked_store, { 0, 0, 0 }, 5, 4, 3) == 0,
            "Alpha-locked painting must not create content on a fully transparent pixel.");
    }

    void TestAirbrushFixedTimestep()
    {
        AirbrushSettings const settings{
            .radius = 2.0F,
            .flow_per_second = 1.0F,
            .fixed_timestep_seconds = 0.1F,
            .hardness = 1.0F,
            .spacing = 10.0F,
            .opacity = 0.8F
        };
        AirbrushAccumulator first(settings);
        AirbrushAccumulator second(settings);
        auto const first_dabs = first.Advance({ 0, 0 }, { 10, 0 }, 0.35, 0.5F, { 20, 40, 60, 255 });
        auto const repeated_dabs = second.Advance({ 0, 0 }, { 10, 0 }, 0.35, 0.5F, { 20, 40, 60, 255 });
        Require(first_dabs.size() == 4 && repeated_dabs.size() == 4,
            "Fixed timestep accumulation plus the initial spatial dab must be deterministic.");
        for (std::size_t index = 0; index < first_dabs.size(); ++index)
        {
            Require(first_dabs[index].center == repeated_dabs[index].center
                && first_dabs[index].opacity == repeated_dabs[index].opacity,
                "Identical airbrush input must produce deterministic positions and opacity.");
            Require(std::abs(first_dabs[index].opacity - 0.04F) < 0.000001F,
                "Airbrush opacity must combine flow, fixed timestep, overall opacity, and pressure.");
        }
        Require(first_dabs[0].center == PointI{ 0, 0 }
            && first_dabs[1].center == PointI{ 3, 0 }
            && first_dabs[2].center == PointI{ 6, 0 }
            && first_dabs[3].center == PointI{ 9, 0 },
            "Airbrush dabs must be placed at deterministic timestep interpolation positions.");
        Require(std::abs(first.PendingSeconds() - 0.05) < 0.000001,
            "Airbrush must retain fractional elapsed time between input events.");

        auto const carried = first.Advance({ 10, 0 }, { 20, 0 }, 0.05, 1.0F, { 20, 40, 60, 255 });
        Require(carried.size() == 1 && carried[0].center == PointI{ 20, 0 },
            "Pending elapsed time must trigger exactly one dab at the next fixed timestep.");
        Require(std::abs(carried[0].opacity - 0.08F) < 0.000001F,
            "Current pressure must determine the emitted fixed-timestep dab opacity.");

        auto const dab_pixels = RasterizeDabs(carried);
        Require(!dab_pixels.empty(), "A valid airbrush dab must rasterize to tile-applicable pixels.");
        for (auto const& pixel : dab_pixels)
        {
            Require(pixel.coverage == 1 && pixel.opacity == carried[0].opacity,
                "Dab rasterization must preserve binary geometry and dab opacity separately.");
        }

        BrushSettings const spacing_settings{
            .radius = 5.0F,
            .flow_per_second = 0.1F,
            .fixed_timestep_seconds = 10.0F,
            .hardness = 1.0F,
            .spacing = 0.5F,
            .opacity = 1.0F,
            .pressure_affects_size = false,
            .pressure_affects_opacity = false
        };
        AirbrushAccumulator one_event(spacing_settings);
        AirbrushAccumulator chunked(spacing_settings);
        auto const complete_path = one_event.Advance({ 0, 0 }, { 100, 0 }, 1.0, 0.2F, {});
        auto first_chunk = chunked.Advance({ 0, 0 }, { 40, 0 }, 0.4, 0.8F, {});
        auto second_chunk = chunked.Advance({ 40, 0 }, { 100, 0 }, 0.6, 0.8F, {});
        first_chunk.insert(first_chunk.end(), second_chunk.begin(), second_chunk.end());
        Require(complete_path.size() == 21 && first_chunk.size() == complete_path.size(),
            "Diameter-ratio spacing must fill fast movement at exact five-pixel intervals.");
        for (std::size_t index = 0; index < complete_path.size(); ++index)
        {
            Require(complete_path[index].center == PointI{ static_cast<std::int32_t>(index * 5), 0 }
                && complete_path[index].center == first_chunk[index].center,
                "Distance spacing must remain stable when the same path is split into input chunks.");
        }

        auto pressure_dab = [](bool const size, bool const opacity)
        {
            AirbrushAccumulator brush({
                .radius = 10.0F,
                .flow_per_second = 0.5F,
                .fixed_timestep_seconds = 1.0F,
                .hardness = 1.0F,
                .spacing = 1.0F,
                .opacity = 0.8F,
                .pressure_affects_size = size,
                .pressure_affects_opacity = opacity
            });
            return brush.Advance(StylusSample{ { 0, 0 }, 0.25F }, StylusSample{ { 0, 0 }, 0.25F }, 0.0, {}).front();
        };
        auto const neither = pressure_dab(false, false);
        auto const size_only = pressure_dab(true, false);
        auto const opacity_only = pressure_dab(false, true);
        auto const both = pressure_dab(true, true);
        Require(neither.radius == 10.0F && std::abs(neither.opacity - 0.4F) < 0.000001F,
            "Disabled pressure mappings must leave size and opacity unchanged.");
        Require(size_only.radius == 2.5F && std::abs(size_only.opacity - 0.4F) < 0.000001F,
            "Pressure size mapping must affect only radius when opacity mapping is disabled.");
        Require(opacity_only.radius == 10.0F && std::abs(opacity_only.opacity - 0.1F) < 0.000001F,
            "Pressure opacity mapping must affect only opacity when size mapping is disabled.");
        Require(both.radius == 2.5F && std::abs(both.opacity - 0.1F) < 0.000001F,
            "Both pressure toggles must map the same normalized pressure independently.");

        std::vector<PaintDab> const soft_dab{
            { .center = { 0, 0 }, .radius = 4.0F, .color = {}, .opacity = 1.0F, .hardness = 0.5F }
        };
        auto const soft_pixels = RasterizeDabs(soft_dab);
        auto opacity_at = [&](PointI const point)
        {
            for (auto const& pixel : soft_pixels)
            {
                if (pixel.position == point)
                {
                    return pixel.opacity;
                }
            }
            throw std::runtime_error("Expected hardness sample was not rasterized.");
        };
        Require(opacity_at({ 0, 0 }) == 1.0F, "Hardness falloff must preserve full center opacity.");
        Require(std::abs(opacity_at({ 3, 0 }) - 0.5F) < 0.000001F,
            "Hardness must create a linear opacity falloff outside the hard core.");
        Require(opacity_at({ 4, 0 }) == 0.0F,
            "The geometric edge must retain binary coverage while its soft opacity falls to zero.");

        RequireInvalidArgument([] { static_cast<void>(AirbrushAccumulator({ .radius = 0.0F })); },
            "Airbrush construction must reject a non-positive radius.");
        RequireInvalidArgument([] { static_cast<void>(AirbrushAccumulator({ .flow_per_second = 1.1F })); },
            "Brush flow must reject values above one.");
        RequireInvalidArgument([] { static_cast<void>(AirbrushAccumulator({ .hardness = -0.1F })); },
            "Brush hardness must reject values below zero.");
        RequireInvalidArgument([] { static_cast<void>(AirbrushAccumulator({ .spacing = 0.0F })); },
            "Brush spacing must reject a zero diameter ratio.");
        RequireInvalidArgument([] { static_cast<void>(AirbrushAccumulator({ .opacity = 1.1F })); },
            "Overall brush opacity must reject values above one.");
        RequireInvalidArgument([&] { static_cast<void>(first.Advance({}, {}, -0.1, 1.0F, {})); },
            "Airbrush input must reject negative elapsed time.");
        RequireInvalidArgument([&] { static_cast<void>(first.Advance({}, {}, 0.1, 1.1F, {})); },
            "Airbrush input must reject pressure above one.");
    }

    void TestStylusSensitivityAndStabilizer()
    {
        PressureMapper const sensitivity({ .minimum_input = 0.2F, .maximum_input = 0.8F, .gamma = 2.0F });
        Require(sensitivity.Map(0.0F) == 0.0F && sensitivity.Map(0.2F) == 0.0F,
            "Pressure mapping must clamp values at and below the configured minimum.");
        Require(std::abs(sensitivity.Map(0.5F) - 0.25F) < 0.000001F,
            "Pressure gamma must shape the normalized input deterministically.");
        Require(sensitivity.Map(0.8F) == 1.0F && sensitivity.Map(1.0F) == 1.0F,
            "Pressure mapping must clamp values at and above the configured maximum.");
        RequireInvalidArgument([] { static_cast<void>(PressureMapper({ .minimum_input = 0.5F, .maximum_input = 0.5F })); },
            "Pressure sensitivity must reject an empty input range.");
        RequireInvalidArgument([] { static_cast<void>(PressureMapper({ .gamma = 0.0F })); },
            "Pressure sensitivity must reject a non-positive gamma.");

        std::vector<StylusSample> const input{
            { { 0, 0 }, 0.5F }, { { 1, 4 }, 0.6F }, { { 2, -4 }, 0.4F },
            { { 3, 4 }, 0.6F }, { { 4, -4 }, 0.4F }, { { 10, 0 }, 0.8F }
        };
        StrokeStabilizer passthrough({ .enabled = false, .strength = 0.9F });
        for (auto const& sample : input)
        {
            Require(passthrough.Push(sample) == sample, "A disabled stroke stabilizer must be exact passthrough.");
        }
        Require(!passthrough.Flush().has_value(), "Passthrough flush must not duplicate an already exact endpoint.");

        StrokeStabilizer first({ .enabled = true, .strength = 0.75F });
        StrokeStabilizer repeated({ .enabled = true, .strength = 0.75F });
        std::int32_t input_jitter = 0;
        std::int32_t output_jitter = 0;
        for (auto const& sample : input)
        {
            auto const first_output = first.Push(sample);
            auto const repeated_output = repeated.Push(sample);
            Require(first_output == repeated_output, "Identical stabilized streams must produce identical samples.");
            input_jitter += std::abs(sample.position.y);
            output_jitter += std::abs(first_output.position.y);
        }
        Require(output_jitter < input_jitter, "Enabled stabilization must reduce high-frequency positional jitter.");
        auto const endpoint = first.Flush();
        auto const repeated_endpoint = repeated.Flush();
        Require(endpoint.has_value() && repeated_endpoint.has_value() && *endpoint == input.back()
            && *repeated_endpoint == input.back(),
            "Stabilizer flush must deterministically preserve the exact final point and pressure.");

        auto const stabilized_pencil = RasterizePencilSamples(input.front(), *endpoint, {});
        Require(stabilized_pencil.front().position == input.front().position
            && stabilized_pencil.back().position == endpoint->position,
            "The shared stylus sample contract must feed Pencil while preserving endpoints.");

        RequireInvalidArgument([] { static_cast<void>(StrokeStabilizer({ .enabled = true, .strength = 1.1F })); },
            "Stroke stabilizer strength must reject values above one.");
        StrokeStabilizer invalid_sample({ .enabled = true, .strength = 0.5F });
        RequireInvalidArgument([&] { static_cast<void>(invalid_sample.Push({ {}, -0.1F })); },
            "Stroke stabilizer input must reject pressure outside zero to one.");
    }

    void TestSelectionRasterization()
    {
        RectI const canvas{ 0, 0, 20, 20 };
        auto const rectangle = RasterizeRectangularSelection(canvas, { 2, 3, 5, 4 });
        Require(rectangle.Bounds() == RectI{ 2, 3, 5, 4 }, "Rectangle mask bounds must be clipped and retained.");
        Require(rectangle.CoverageAt({ 2, 3 }) == 1 && rectangle.CoverageAt({ 6, 6 }) == 1,
            "Rectangle selection must include its first and last covered boundary pixels.");
        Require(rectangle.CoverageAt({ 1, 3 }) == 0 && rectangle.CoverageAt({ 7, 6 }) == 0,
            "Rectangle selection must exclude pixels outside its half-open bounds.");

        auto const ellipse = RasterizeEllipticalSelection(canvas, { 2, 2, 5, 5 });
        Require(ellipse.CoverageAt({ 4, 4 }) == 1, "Ellipse selection must include its center.");
        Require(ellipse.CoverageAt({ 2, 4 }) == 1 && ellipse.CoverageAt({ 6, 4 }) == 1,
            "Ellipse selection must include exact axis boundary pixels.");
        Require(ellipse.CoverageAt({ 2, 2 }) == 0 && ellipse.CoverageAt({ 7, 4 }) == 0,
            "Ellipse selection must exclude bounding-box corners and exterior pixels.");

        std::vector<PointI> const triangle{ { 2, 2 }, { 8, 2 }, { 5, 8 } };
        auto const polygon = RasterizePolygonalSelection(canvas, triangle);
        Require(polygon.CoverageAt({ 5, 4 }) == 1, "Polygon selection must include interior pixels.");
        Require(polygon.CoverageAt({ 5, 2 }) == 1 && polygon.CoverageAt({ 2, 2 }) == 1,
            "Polygon selection must include edge and vertex boundary pixels.");
        Require(polygon.CoverageAt({ 2, 7 }) == 0, "Polygon selection must exclude exterior pixels inside its bounding rectangle.");

        std::vector<PointI> const freehand{ { 10, 10 }, { 14, 10 }, { 14, 14 }, { 10, 14 }, { 10, 10 } };
        auto const lasso = RasterizeFreehandSelection(canvas, freehand);
        Require(lasso.CoverageAt({ 12, 12 }) == 1 && lasso.CoverageAt({ 10, 12 }) == 1,
            "A freehand closed path must include its interior and boundary.");
        Require(lasso.CoverageAt({ 9, 12 }) == 0, "A freehand closed path must exclude its exterior.");

        auto const clipped = RasterizeRectangularSelection(canvas, { -3, -3, 5, 5 });
        Require(clipped.Bounds() == RectI{ 0, 0, 2, 2 } && clipped.CoverageAt({ 0, 0 }) == 1,
            "Selection masks must clip safely to canvas bounds.");
        RequireInvalidArgument([&] { static_cast<void>(RasterizeRectangularSelection(canvas, { 0, 0, 0, 4 })); },
            "Rectangle selection must reject empty bounds.");
        RequireInvalidArgument([&] { static_cast<void>(RasterizePolygonalSelection(canvas, std::span<PointI const>(triangle.data(), 2))); },
            "Polygon selection must reject fewer than three vertices.");
        std::vector<PointI> const collinear{ { 1, 1 }, { 2, 2 }, { 3, 3 } };
        RequireInvalidArgument([&] { static_cast<void>(RasterizeFreehandSelection(canvas, collinear)); },
            "Closed selection paths must reject zero-area input.");
    }
}

int main()
{
    try
    {
        TestToolAndColorContracts();
        TestPencilBresenhamAndTileApplication();
        TestAirbrushFixedTimestep();
        TestStylusSensitivityAndStabilizer();
        TestSelectionRasterization();
        std::cout << "OctoPaint tool tests passed.\n";
        return EXIT_SUCCESS;
    }
    catch (std::exception const& error)
    {
        std::cerr << "OctoPaint tool tests failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
