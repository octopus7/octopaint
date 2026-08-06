#include "pch.h"
#include "DockablePanelController.h"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace octopaint::winui
{
    namespace
    {
        [[nodiscard]] winrt::hstring PanelAutomationName(
            wchar_t const* action,
            winrt::hstring const& title)
        {
            return winrt::hstring{ std::wstring{ action } + L" " + title.c_str() + L" panel" };
        }
    }

    DockablePanelController::~DockablePanelController()
    {
        Shutdown();
    }

    void DockablePanelController::Register(DockablePanelDescriptor descriptor)
    {
        if (descriptor.id.empty() || !descriptor.panel || !descriptor.dock_host ||
            !descriptor.toggle_button || !descriptor.dock_column || descriptor.docked_width <= 0.0)
        {
            throw std::invalid_argument("A dockable panel descriptor is incomplete.");
        }
        if (Find(descriptor.id))
        {
            throw std::invalid_argument("A dockable panel ID must be unique.");
        }
        if (descriptor.dock_host.Content() != descriptor.panel)
        {
            throw std::invalid_argument("The dock host must initially contain the registered panel.");
        }

        descriptor.toggle_button.Content(winrt::box_value(L"Float"));
        winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(
            descriptor.toggle_button,
            PanelAutomationName(L"Float", descriptor.title));
        entries_.push_back({ std::move(descriptor) });
    }

    void DockablePanelController::Toggle(winrt::hstring const& id)
    {
        if (IsFloating(id))
        {
            Dock(id);
        }
        else
        {
            Float(id);
        }
    }

    void DockablePanelController::Float(winrt::hstring const& id)
    {
        auto* entry = Find(id);
        if (!entry || entry->floating_window || shutting_down_)
        {
            return;
        }

        auto window = winrt::Microsoft::UI::Xaml::Window{};
        try
        {
            window.Title(entry->descriptor.title);
            entry->descriptor.dock_host.Content(nullptr);
            entry->descriptor.dock_column.Width(winrt::Microsoft::UI::Xaml::GridLength{ 0.0 });
            window.Content(entry->descriptor.panel);
            entry->descriptor.toggle_button.Content(winrt::box_value(L"Dock"));
            winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(
                entry->descriptor.toggle_button,
                PanelAutomationName(L"Dock", entry->descriptor.title));

            auto const panel_id = entry->descriptor.id;
            auto const weak_lifetime = std::weak_ptr<int>{ lifetime_token_ };
            entry->closed_token = window.Closed(
                [this, panel_id](
                    auto const& sender,
                    [[maybe_unused]] winrt::Microsoft::UI::Xaml::WindowEventArgs const& event_args)
                {
                    FloatingWindowClosed(
                        panel_id,
                        sender.template as<winrt::Microsoft::UI::Xaml::Window>());
                });
            entry->app_closing_token = window.AppWindow().Closing(
                [this, panel_id, weak_lifetime, dispatcher = window.DispatcherQueue()](
                    [[maybe_unused]] auto const& sender,
                    winrt::Microsoft::UI::Windowing::AppWindowClosingEventArgs const& event_args)
                {
                    if (shutting_down_)
                    {
                        return;
                    }
                    event_args.Cancel(true);
                    [[maybe_unused]] auto const enqueued = dispatcher.TryEnqueue(
                        [this, panel_id, weak_lifetime]
                    {
                        if (weak_lifetime.lock())
                        {
                            Dock(panel_id);
                        }
                    });
                });
            entry->floating_window = window;
            window.AppWindow().Resize({
                entry->descriptor.floating_width,
                entry->descriptor.floating_height });
            window.Activate();
        }
        catch (...)
        {
            try
            {
                if (entry->closed_token.value != 0)
                {
                    window.Closed(entry->closed_token);
                }
                if (entry->app_closing_token.value != 0)
                {
                    window.AppWindow().Closing(entry->app_closing_token);
                }
                window.Content(nullptr);
                entry->floating_window = nullptr;
                entry->closed_token = {};
                entry->app_closing_token = {};
                RestoreDockedContent(*entry);
                window.Close();
            }
            catch (...)
            {
                // The panel is restored whenever the XAML tree remains usable.
            }
        }
    }

    void DockablePanelController::Dock(winrt::hstring const& id)
    {
        auto* entry = Find(id);
        if (!entry || !entry->floating_window)
        {
            return;
        }

        auto const window = entry->floating_window;
        window.Closed(entry->closed_token);
        window.AppWindow().Closing(entry->app_closing_token);
        window.Content(nullptr);
        entry->floating_window = nullptr;
        entry->closed_token = {};
        entry->app_closing_token = {};
        RestoreDockedContent(*entry);
        window.Close();
    }

    void DockablePanelController::Shutdown() noexcept
    {
        if (shutting_down_)
        {
            return;
        }
        shutting_down_ = true;
        lifetime_token_.reset();

        for (auto& entry : entries_)
        {
            try
            {
                if (entry.floating_window)
                {
                    auto const window = entry.floating_window;
                    window.Closed(entry.closed_token);
                    window.AppWindow().Closing(entry.app_closing_token);
                    window.Content(nullptr);
                    entry.floating_window = nullptr;
                    entry.closed_token = {};
                    entry.app_closing_token = {};
                    window.Close();
                }
            }
            catch (...)
            {
                // Window teardown must not escape a MainWindow destructor.
            }
        }
    }

    bool DockablePanelController::IsFloating(winrt::hstring const& id) const noexcept
    {
        auto const* entry = Find(id);
        return entry && entry->floating_window;
    }

    DockablePanelController::Entry* DockablePanelController::Find(winrt::hstring const& id) noexcept
    {
        auto const found = std::ranges::find_if(entries_, [&id](Entry const& entry)
        {
            return entry.descriptor.id == id;
        });
        return found == entries_.end() ? nullptr : &*found;
    }

    DockablePanelController::Entry const* DockablePanelController::Find(winrt::hstring const& id) const noexcept
    {
        auto const found = std::ranges::find_if(entries_, [&id](Entry const& entry)
        {
            return entry.descriptor.id == id;
        });
        return found == entries_.end() ? nullptr : &*found;
    }

    void DockablePanelController::RestoreDockedContent(Entry& entry)
    {
        entry.descriptor.dock_host.Content(entry.descriptor.panel);
        entry.descriptor.dock_column.Width(
            winrt::Microsoft::UI::Xaml::GridLength{ entry.descriptor.docked_width });
        entry.descriptor.toggle_button.Content(winrt::box_value(L"Float"));
        winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(
            entry.descriptor.toggle_button,
            PanelAutomationName(L"Float", entry.descriptor.title));
    }

    void DockablePanelController::FloatingWindowClosed(
        winrt::hstring const& id,
        winrt::Microsoft::UI::Xaml::Window const& window)
    {
        auto* entry = Find(id);
        if (!entry || shutting_down_)
        {
            return;
        }

        window.Content(nullptr);
        entry->floating_window = nullptr;
        entry->closed_token = {};
        entry->app_closing_token = {};
        RestoreDockedContent(*entry);
    }
}
