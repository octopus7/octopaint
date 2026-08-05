#pragma once

#include <cstdint>

namespace octopaint::winui
{
    // UI-local projection until the application layer exposes a tool-options port.
    // Keeping this value-only state independent of XAML controls makes that handoff explicit.
    struct ToolOptionsState final
    {
        double brush_size{ 32.0 };
        double hardness{ 80.0 };
        double spacing{ 10.0 };
        double flow{ 100.0 };
        double opacity{ 100.0 };

        bool pressure_affects_dab_size{ true };
        bool pressure_affects_opacity{};
        bool stroke_stabilizer{};

        std::int32_t pressure_curve_preset{};
        double pressure_gamma{ 1.0 };
        double stabilizer_strength{ 50.0 };
        double stabilizer_smoothing{ 50.0 };
    };
}
