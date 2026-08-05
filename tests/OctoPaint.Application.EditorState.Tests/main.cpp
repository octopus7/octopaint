#include <octopaint/application/EditorState.h>

#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace
{
    using namespace octopaint::application;

    void Require(bool const condition, std::string_view const message)
    {
        if (!condition)
        {
            throw std::runtime_error(std::string{ message });
        }
    }

    template<typename Callback>
    void RequireInvalid(Callback&& callback, std::string_view const message)
    {
        try
        {
            callback();
        }
        catch (std::invalid_argument const&)
        {
            return;
        }
        throw std::runtime_error(std::string{ message });
    }

    void TestDefaults()
    {
        EditorState state;
        auto const snapshot = state.Snapshot();

        Require(snapshot.Revision() == 0, "Default editor state revision must be zero.");
        Require(snapshot.ActiveTool() == EditorTool::Pencil, "Pencil must be the default tool.");
        Require(snapshot.ForegroundColor() == EditorColor{ 0, 0, 0, 255 }, "Foreground must default to opaque black.");
        Require(snapshot.BackgroundColor() == EditorColor{ 255, 255, 255, 255 }, "Background must default to opaque white.");
        Require(snapshot.Brush() == BrushOptions{}, "Brush options must have stable defaults.");
        Require(snapshot.Stabilizer() == StabilizerOptions{}, "Stabilizer options must have stable defaults.");
        Require(snapshot.Pressure() == PressureOptions{}, "Pressure options must have stable defaults.");
    }

    void TestMutationsAndRevision()
    {
        EditorState state;
        state.SetActiveTool(EditorTool::Airbrush);
        Require(state.Snapshot().Revision() == 1, "A changed tool must advance the revision.");
        state.SetActiveTool(EditorTool::Airbrush);
        Require(state.Snapshot().Revision() == 1, "Setting the same tool must not advance the revision.");

        EditorColor const red{ 220, 10, 20, 200 };
        EditorColor const blue{ 10, 20, 220, 180 };
        state.SetForegroundColor(red);
        state.SetBackgroundColor(blue);
        state.SwapColors();
        auto swapped = state.Snapshot();
        Require(swapped.ForegroundColor() == blue && swapped.BackgroundColor() == red,
            "SwapColors must exchange the full RGBA values.");
        state.ResetColors();
        auto reset = state.Snapshot();
        Require(reset.ForegroundColor() == EditorColor{ 0, 0, 0, 255 }
            && reset.BackgroundColor() == EditorColor{ 255, 255, 255, 255 },
            "ResetColors must restore black and white.");

        BrushOptions const brush{
            .size_pixels = 48.0F,
            .hardness = 0.35F,
            .spacing = 0.12F,
            .flow = 0.65F,
            .opacity = 0.8F,
            .pressure_affects_size = true,
            .pressure_affects_opacity = false
        };
        state.SetBrush(brush);
        Require(state.Snapshot().Brush() == brush, "All brush options must be retained exactly.");
        auto const brush_revision = state.Snapshot().Revision();
        state.SetBrush(brush);
        Require(state.Snapshot().Revision() == brush_revision, "An identical brush DTO must not emit a change.");

        state.SetBrushSize(64.0F);
        state.SetBrushHardness(0.5F);
        state.SetBrushSpacing(0.2F);
        state.SetBrushFlow(0.7F);
        state.SetBrushOpacity(0.9F);
        state.SetPressureAffectsSize(false);
        state.SetPressureAffectsOpacity(true);
        auto const individually_set = state.Snapshot().Brush();
        Require(individually_set.size_pixels == 64.0F
            && individually_set.hardness == 0.5F
            && individually_set.spacing == 0.2F
            && individually_set.flow == 0.7F
            && individually_set.opacity == 0.9F
            && !individually_set.pressure_affects_size
            && individually_set.pressure_affects_opacity,
            "Individual brush setters must update their corresponding fields.");

        StabilizerOptions const stabilizer{ .enabled = true, .strength = 0.75F, .smoothing = 0.4F };
        state.SetStabilizer(stabilizer);
        Require(state.Snapshot().Stabilizer() == stabilizer, "Stabilizer settings must be retained exactly.");
        state.SetStabilizerEnabled(false);
        state.SetStabilizerStrength(0.25F);
        state.SetStabilizerSmoothing(0.6F);
        Require(state.Snapshot().Stabilizer() == StabilizerOptions{ false, 0.25F, 0.6F },
            "Individual stabilizer setters must update their corresponding fields.");

        PressureOptions const pressure{ .minimum_input = 0.1F, .maximum_input = 0.9F, .gamma = 1.8F };
        state.SetPressure(pressure);
        Require(state.Snapshot().Pressure() == pressure, "Pressure settings must be retained exactly.");
        state.SetPressureGamma(2.0F);
        state.SetPressureRange(0.2F, 0.8F);
        Require(state.Snapshot().Pressure() == PressureOptions{ 0.2F, 0.8F, 2.0F },
            "Individual pressure setters must update their corresponding fields.");
    }

    void TestValidationIsAtomic()
    {
        EditorState state;
        auto const before = state.Snapshot();
        auto const nan = std::numeric_limits<float>::quiet_NaN();

        RequireInvalid([&] { state.SetActiveTool(static_cast<EditorTool>(255)); }, "Unknown tool values must be rejected.");
        RequireInvalid([&] { state.SetBrushSize(0.0F); }, "Zero brush size must be rejected.");
        RequireInvalid([&] { state.SetBrushHardness(1.01F); }, "Hardness above one must be rejected.");
        RequireInvalid([&] { state.SetBrushSpacing(0.0F); }, "Zero spacing must be rejected.");
        RequireInvalid([&] { state.SetBrushFlow(-0.01F); }, "Negative flow must be rejected.");
        RequireInvalid([&] { state.SetBrushOpacity(nan); }, "Non-finite opacity must be rejected.");
        RequireInvalid([&] { state.SetStabilizerStrength(-0.1F); }, "Negative stabilizer strength must be rejected.");
        RequireInvalid([&] { state.SetStabilizerSmoothing(1.1F); }, "Smoothing above one must be rejected.");
        RequireInvalid([&] { state.SetPressureRange(0.5F, 0.5F); }, "Equal pressure bounds must be rejected.");
        RequireInvalid([&] { state.SetPressureRange(-0.1F, 0.9F); }, "Pressure bounds outside zero to one must be rejected.");
        RequireInvalid([&] { state.SetPressureGamma(0.0F); }, "Zero pressure gamma must be rejected.");
        RequireInvalid([&] { state.SetPressureGamma(nan); }, "Non-finite pressure gamma must be rejected.");

        auto const after = state.Snapshot();
        Require(after.Revision() == before.Revision(), "Rejected input must not advance the revision.");
        Require(after.ActiveTool() == before.ActiveTool()
            && after.Brush() == before.Brush()
            && after.Stabilizer() == before.Stabilizer()
            && after.Pressure() == before.Pressure(),
            "Rejected input must not partially mutate editor state.");

        state.SetBrush(BrushOptions{ .size_pixels = 0.02F, .hardness = 0.0F, .spacing = 0.01F, .flow = 0.0F, .opacity = 0.0F });
        state.SetStabilizer(StabilizerOptions{ .enabled = true, .strength = 1.0F, .smoothing = 1.0F });
        state.SetPressure(PressureOptions{ .minimum_input = 0.0F, .maximum_input = 1.0F, .gamma = 100.0F });
        Require(state.Snapshot().Revision() == 3, "Valid Core boundary values must be accepted.");
    }

    void TestSnapshotIsDetached()
    {
        EditorState state;
        auto const old_snapshot = state.Snapshot();
        state.SetActiveTool(EditorTool::MoveLayer);
        state.SetBrushSize(32.0F);
        auto const new_snapshot = state.Snapshot();

        Require(old_snapshot.Revision() == 0
            && old_snapshot.ActiveTool() == EditorTool::Pencil
            && old_snapshot.Brush().size_pixels == 16.0F,
            "An earlier snapshot must remain unchanged after live state mutations.");
        Require(new_snapshot.Revision() == 2
            && new_snapshot.ActiveTool() == EditorTool::MoveLayer
            && new_snapshot.Brush().size_pixels == 32.0F,
            "A new snapshot must contain the current values.");
    }
}

int main()
{
    try
    {
        TestDefaults();
        TestMutationsAndRevision();
        TestValidationIsAtomic();
        TestSnapshotIsDetached();
        std::cout << "OctoPaint application editor state tests passed.\n";
        return EXIT_SUCCESS;
    }
    catch (std::exception const& error)
    {
        std::cerr << "OctoPaint application editor state tests failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
