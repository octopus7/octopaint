#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Dispatching.h>

namespace octopaint::winui
{
    struct DockablePanelDescriptor final
    {
        winrt::hstring id;
        winrt::hstring title;
        winrt::Microsoft::UI::Xaml::FrameworkElement panel{ nullptr };
        winrt::Microsoft::UI::Xaml::Controls::ContentControl dock_host{ nullptr };
        winrt::Microsoft::UI::Xaml::Controls::Button toggle_button{ nullptr };
        winrt::Microsoft::UI::Xaml::Controls::ColumnDefinition dock_column{ nullptr };
        double docked_width{};
        std::int32_t floating_width{ 320 };
        std::int32_t floating_height{ 640 };
    };

    // Moves registered panel content between its main-window host and a
    // dedicated WinUI Window. Descriptors make additional side panels opt-in
    // without coupling this controller to MainWindow's generated XAML type.
    class DockablePanelController final
    {
    public:
        DockablePanelController() = default;
        ~DockablePanelController();

        DockablePanelController(DockablePanelController const&) = delete;
        DockablePanelController& operator=(DockablePanelController const&) = delete;

        void Register(DockablePanelDescriptor descriptor);
        void RequestToggle(
            winrt::hstring const& id,
            winrt::Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher) noexcept;
        void Toggle(winrt::hstring const& id) noexcept;
        void Float(winrt::hstring const& id) noexcept;
        void Dock(winrt::hstring const& id) noexcept;
        void Shutdown() noexcept;

        [[nodiscard]] bool IsFloating(winrt::hstring const& id) const noexcept;

    private:
        struct Entry final
        {
            DockablePanelDescriptor descriptor;
            winrt::Microsoft::UI::Xaml::Window floating_window{ nullptr };
            winrt::event_token closed_token{};
            winrt::event_token app_closing_token{};
            bool transition_pending{};
            bool closing_internally{};
        };

        [[nodiscard]] Entry* Find(winrt::hstring const& id) noexcept;
        [[nodiscard]] Entry const* Find(winrt::hstring const& id) const noexcept;
        void RestoreDockedContent(Entry& entry);
        void FloatingWindowClosed(
            winrt::hstring const& id,
            winrt::Microsoft::UI::Xaml::Window const& window) noexcept;
        void SetFloatingButton(Entry& entry) noexcept;
        void SetDockedButton(Entry& entry) noexcept;
        [[nodiscard]] bool RestoreFloatingContent(
            Entry& entry,
            winrt::Microsoft::UI::Xaml::Window const& window) noexcept;

        std::vector<Entry> entries_;
        std::shared_ptr<int> lifetime_token_{ std::make_shared<int>(0) };
        bool shutting_down_{};
    };
}
