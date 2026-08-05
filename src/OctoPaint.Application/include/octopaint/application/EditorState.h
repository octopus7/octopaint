#pragma once

#include <compare>
#include <cstdint>

namespace octopaint::application
{
    enum class EditorTool : std::uint8_t
    {
        Pencil,
        Airbrush,
        RectangularMarquee,
        EllipticalMarquee,
        FreehandLasso,
        PolygonalLasso,
        MoveLayer
    };

    struct EditorColor final
    {
        std::uint8_t red{};
        std::uint8_t green{};
        std::uint8_t blue{};
        std::uint8_t alpha{ 255 };

        auto operator<=>(EditorColor const&) const = default;
    };

    struct BrushOptions final
    {
        // Size is the dab diameter in document pixels. Spacing is a ratio of
        // the pressure-adjusted diameter. Flow and opacity are normalized.
        float size_pixels{ 16.0F };
        float hardness{ 1.0F };
        float spacing{ 0.25F };
        float flow{ 1.0F };
        float opacity{ 1.0F };
        bool pressure_affects_size{};
        bool pressure_affects_opacity{ true };

        bool operator==(BrushOptions const&) const = default;
    };

    struct StabilizerOptions final
    {
        bool enabled{};
        float strength{ 0.5F };
        // Smoothing is a frontend-neutral normalized quality/latency setting.
        // The stroke engine decides how it maps to its sampling implementation.
        float smoothing{ 0.5F };

        bool operator==(StabilizerOptions const&) const = default;
    };

    struct PressureOptions final
    {
        float minimum_input{};
        float maximum_input{ 1.0F };
        float gamma{ 1.0F };

        bool operator==(PressureOptions const&) const = default;
    };

    // A detached immutable view. Its members are accessible only through const
    // accessors, and it never refers back to the live EditorState.
    class EditorStateSnapshot final
    {
    public:
        [[nodiscard]] std::uint64_t Revision() const noexcept;
        [[nodiscard]] EditorTool ActiveTool() const noexcept;
        [[nodiscard]] EditorColor ForegroundColor() const noexcept;
        [[nodiscard]] EditorColor BackgroundColor() const noexcept;
        [[nodiscard]] BrushOptions const& Brush() const noexcept;
        [[nodiscard]] StabilizerOptions const& Stabilizer() const noexcept;
        [[nodiscard]] PressureOptions const& Pressure() const noexcept;

    private:
        EditorStateSnapshot(
            std::uint64_t revision,
            EditorTool active_tool,
            EditorColor foreground,
            EditorColor background,
            BrushOptions brush,
            StabilizerOptions stabilizer,
            PressureOptions pressure) noexcept;

        std::uint64_t revision_{};
        EditorTool active_tool_{ EditorTool::Pencil };
        EditorColor foreground_{};
        EditorColor background_{ 255, 255, 255, 255 };
        BrushOptions brush_;
        StabilizerOptions stabilizer_;
        PressureOptions pressure_;

        friend class EditorState;
    };

    // EditorState is session/preference state. It is intentionally independent
    // from Workspace document history, saved revisions, and dirty tracking.
    class EditorState final
    {
    public:
        EditorState() = default;

        [[nodiscard]] EditorStateSnapshot Snapshot() const noexcept;

        void SetActiveTool(EditorTool tool);
        void SetForegroundColor(EditorColor color);
        void SetBackgroundColor(EditorColor color);
        void SwapColors();
        void ResetColors();

        void SetBrush(BrushOptions options);
        void SetBrushSize(float size_pixels);
        void SetBrushHardness(float hardness);
        void SetBrushSpacing(float spacing);
        void SetBrushFlow(float flow);
        void SetBrushOpacity(float opacity);
        void SetPressureAffectsSize(bool enabled);
        void SetPressureAffectsOpacity(bool enabled);

        void SetStabilizer(StabilizerOptions options);
        void SetStabilizerEnabled(bool enabled);
        void SetStabilizerStrength(float strength);
        void SetStabilizerSmoothing(float smoothing);

        void SetPressure(PressureOptions options);
        void SetPressureGamma(float gamma);
        void SetPressureRange(float minimum_input, float maximum_input);

    private:
        void BumpRevision();

        std::uint64_t revision_{};
        EditorTool active_tool_{ EditorTool::Pencil };
        EditorColor foreground_{};
        EditorColor background_{ 255, 255, 255, 255 };
        BrushOptions brush_;
        StabilizerOptions stabilizer_;
        PressureOptions pressure_;
    };
}
