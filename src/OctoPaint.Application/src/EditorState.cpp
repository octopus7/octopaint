#include <octopaint/application/EditorState.h>

#include <octopaint/core/Tools.h>

#include <cmath>
#include <limits>
#include <stdexcept>

namespace
{
    using namespace octopaint::application;

    [[nodiscard]] octopaint::core::ToolKind ToCoreTool(EditorTool const tool)
    {
        using octopaint::core::ToolKind;
        switch (tool)
        {
        case EditorTool::Pencil: return ToolKind::Pencil;
        case EditorTool::Airbrush: return ToolKind::Airbrush;
        case EditorTool::RectangularMarquee: return ToolKind::RectangularMarquee;
        case EditorTool::EllipticalMarquee: return ToolKind::EllipticalMarquee;
        case EditorTool::FreehandLasso: return ToolKind::FreehandLasso;
        case EditorTool::PolygonalLasso: return ToolKind::PolygonalLasso;
        case EditorTool::MoveLayer: return ToolKind::MoveLayer;
        }
        throw std::invalid_argument("The editor tool value is not recognized.");
    }

    void ValidateBrush(BrushOptions const& options)
    {
        // Constructing the Core DTO here keeps the mapping private while using
        // the paint engine's validation contract as the source of truth.
        auto const radius = options.size_pixels * 0.5F;
        [[maybe_unused]] octopaint::core::AirbrushAccumulator const validator{
            octopaint::core::BrushSettings{
                .radius = radius,
                .flow_per_second = options.flow,
                .hardness = options.hardness,
                .spacing = options.spacing,
                .opacity = options.opacity,
                .pressure_affects_size = options.pressure_affects_size,
                .pressure_affects_opacity = options.pressure_affects_opacity
            }
        };
    }

    void ValidateStabilizer(StabilizerOptions const& options)
    {
        [[maybe_unused]] octopaint::core::StrokeStabilizer const validator{
            octopaint::core::StrokeStabilizerSettings{
                .enabled = options.enabled,
                .strength = options.strength
            }
        };
        if (!std::isfinite(options.smoothing)
            || options.smoothing < 0.0F || options.smoothing > 1.0F)
        {
            throw std::invalid_argument("Stroke stabilizer smoothing must be finite and between zero and one.");
        }
    }

    void ValidatePressure(PressureOptions const& options)
    {
        [[maybe_unused]] octopaint::core::PressureMapper const validator{
            octopaint::core::PressureSensitivity{
                .minimum_input = options.minimum_input,
                .maximum_input = options.maximum_input,
                .gamma = options.gamma
            }
        };
    }
}

namespace octopaint::application
{
    EditorStateSnapshot::EditorStateSnapshot(
        std::uint64_t const revision,
        EditorTool const active_tool,
        EditorColor const foreground,
        EditorColor const background,
        BrushOptions const brush,
        StabilizerOptions const stabilizer,
        PressureOptions const pressure) noexcept
        : revision_(revision),
          active_tool_(active_tool),
          foreground_(foreground),
          background_(background),
          brush_(brush),
          stabilizer_(stabilizer),
          pressure_(pressure)
    {
    }

    std::uint64_t EditorStateSnapshot::Revision() const noexcept { return revision_; }
    EditorTool EditorStateSnapshot::ActiveTool() const noexcept { return active_tool_; }
    EditorColor EditorStateSnapshot::ForegroundColor() const noexcept { return foreground_; }
    EditorColor EditorStateSnapshot::BackgroundColor() const noexcept { return background_; }
    BrushOptions const& EditorStateSnapshot::Brush() const noexcept { return brush_; }
    StabilizerOptions const& EditorStateSnapshot::Stabilizer() const noexcept { return stabilizer_; }
    PressureOptions const& EditorStateSnapshot::Pressure() const noexcept { return pressure_; }

    EditorStateSnapshot EditorState::Snapshot() const noexcept
    {
        return {
            revision_,
            active_tool_,
            foreground_,
            background_,
            brush_,
            stabilizer_,
            pressure_
        };
    }

    void EditorState::BumpRevision()
    {
        if (revision_ == std::numeric_limits<std::uint64_t>::max())
        {
            throw std::overflow_error("The editor state revision is exhausted.");
        }
        ++revision_;
    }

    void EditorState::SetActiveTool(EditorTool const tool)
    {
        [[maybe_unused]] auto const mapped_tool = ToCoreTool(tool);
        if (active_tool_ != tool)
        {
            BumpRevision();
            active_tool_ = tool;
        }
    }

    void EditorState::SetForegroundColor(EditorColor const color)
    {
        if (foreground_ != color)
        {
            BumpRevision();
            foreground_ = color;
        }
    }

    void EditorState::SetBackgroundColor(EditorColor const color)
    {
        if (background_ != color)
        {
            BumpRevision();
            background_ = color;
        }
    }

    void EditorState::SwapColors()
    {
        if (foreground_ != background_)
        {
            BumpRevision();
            auto const old_foreground = foreground_;
            foreground_ = background_;
            background_ = old_foreground;
        }
    }

    void EditorState::ResetColors()
    {
        constexpr EditorColor default_foreground{ 0, 0, 0, 255 };
        constexpr EditorColor default_background{ 255, 255, 255, 255 };
        if (foreground_ != default_foreground || background_ != default_background)
        {
            BumpRevision();
            foreground_ = default_foreground;
            background_ = default_background;
        }
    }

    void EditorState::SetBrush(BrushOptions const options)
    {
        ValidateBrush(options);
        if (brush_ != options)
        {
            BumpRevision();
            brush_ = options;
        }
    }

    void EditorState::SetBrushSize(float const size_pixels)
    {
        auto options = brush_;
        options.size_pixels = size_pixels;
        SetBrush(options);
    }

    void EditorState::SetBrushHardness(float const hardness)
    {
        auto options = brush_;
        options.hardness = hardness;
        SetBrush(options);
    }

    void EditorState::SetBrushSpacing(float const spacing)
    {
        auto options = brush_;
        options.spacing = spacing;
        SetBrush(options);
    }

    void EditorState::SetBrushFlow(float const flow)
    {
        auto options = brush_;
        options.flow = flow;
        SetBrush(options);
    }

    void EditorState::SetBrushOpacity(float const opacity)
    {
        auto options = brush_;
        options.opacity = opacity;
        SetBrush(options);
    }

    void EditorState::SetPressureAffectsSize(bool const enabled)
    {
        auto options = brush_;
        options.pressure_affects_size = enabled;
        SetBrush(options);
    }

    void EditorState::SetPressureAffectsOpacity(bool const enabled)
    {
        auto options = brush_;
        options.pressure_affects_opacity = enabled;
        SetBrush(options);
    }

    void EditorState::SetStabilizer(StabilizerOptions const options)
    {
        ValidateStabilizer(options);
        if (stabilizer_ != options)
        {
            BumpRevision();
            stabilizer_ = options;
        }
    }

    void EditorState::SetStabilizerEnabled(bool const enabled)
    {
        auto options = stabilizer_;
        options.enabled = enabled;
        SetStabilizer(options);
    }

    void EditorState::SetStabilizerStrength(float const strength)
    {
        auto options = stabilizer_;
        options.strength = strength;
        SetStabilizer(options);
    }

    void EditorState::SetStabilizerSmoothing(float const smoothing)
    {
        auto options = stabilizer_;
        options.smoothing = smoothing;
        SetStabilizer(options);
    }

    void EditorState::SetPressure(PressureOptions const options)
    {
        ValidatePressure(options);
        if (pressure_ != options)
        {
            BumpRevision();
            pressure_ = options;
        }
    }

    void EditorState::SetPressureGamma(float const gamma)
    {
        auto options = pressure_;
        options.gamma = gamma;
        SetPressure(options);
    }

    void EditorState::SetPressureRange(float const minimum_input, float const maximum_input)
    {
        auto options = pressure_;
        options.minimum_input = minimum_input;
        options.maximum_input = maximum_input;
        SetPressure(options);
    }
}
