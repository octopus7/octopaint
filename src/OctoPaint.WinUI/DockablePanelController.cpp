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

    void DockablePanelController::RequestToggle(
        winrt::hstring const& id,
        winrt::Microsoft::UI::Dispatching::DispatcherQueue const& dispatcher) noexcept
    {
        auto* entry = Find(id);
        if (!entry || entry->transition_pending || shutting_down_ || !dispatcher)
        {
            return;
        }

        entry->transition_pending = true;
        auto const weak_lifetime = std::weak_ptr<int>{ lifetime_token_ };
        try
        {
            auto const enqueued = dispatcher.TryEnqueue(
                [this, id, weak_lifetime]
                {
                    auto const lifetime = weak_lifetime.lock();
                    if (!lifetime)
                    {
                        return;
                    }

                    Toggle(id);
                    if (auto* current = Find(id))
                    {
                        current->transition_pending = false;
                    }
                });
            if (!enqueued)
            {
                entry->transition_pending = false;
            }
        }
        catch (...)
        {
            entry->transition_pending = false;
        }
    }

    void DockablePanelController::Toggle(winrt::hstring const& id) noexcept
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

    void DockablePanelController::Float(winrt::hstring const& id) noexcept
    {
        auto* entry = Find(id);
        if (!entry || entry->floating_window || shutting_down_)
        {
            return;
        }

        winrt::Microsoft::UI::Xaml::Window window{ nullptr };
        try
        {
            window = winrt::Microsoft::UI::Xaml::Window{};
            window.Title(entry->descriptor.title);
            entry->descriptor.dock_host.Content(nullptr);
            entry->descriptor.dock_column.Width(winrt::Microsoft::UI::Xaml::GridLength{ 0.0 });
            window.Content(entry->descriptor.panel);
            SetFloatingButton(*entry);

            auto const panel_id = entry->descriptor.id;
            auto const weak_lifetime = std::weak_ptr<int>{ lifetime_token_ };
            entry->closed_token = window.Closed(
                [this, panel_id, weak_lifetime](
                    auto const& sender,
                    [[maybe_unused]] winrt::Microsoft::UI::Xaml::WindowEventArgs const& event_args)
                {
                    auto const lifetime = weak_lifetime.lock();
                    if (!lifetime)
                    {
                        return;
                    }
                    FloatingWindowClosed(
                        panel_id,
                        sender.template as<winrt::Microsoft::UI::Xaml::Window>());
                });
            entry->app_closing_token = window.AppWindow().Closing(
                [this, panel_id, weak_lifetime, dispatcher = window.DispatcherQueue()](
                    [[maybe_unused]] auto const& sender,
                    winrt::Microsoft::UI::Windowing::AppWindowClosingEventArgs const& event_args)
                {
                    auto const lifetime = weak_lifetime.lock();
                    if (!lifetime)
                    {
                        return;
                    }

                    auto* current = Find(panel_id);
                    if (!current || shutting_down_ || current->closing_internally)
                    {
                        return;
                    }

                    event_args.Cancel(true);
                    if (current->transition_pending)
                    {
                        return;
                    }

                    current->transition_pending = true;
                    auto enqueued = false;
                    try
                    {
                        enqueued = dispatcher.TryEnqueue(
                            [this, panel_id, weak_lifetime]
                            {
                                auto const callback_lifetime = weak_lifetime.lock();
                                if (!callback_lifetime)
                                {
                                    return;
                                }

                                Dock(panel_id);
                                if (auto* pending_entry = Find(panel_id))
                                {
                                    pending_entry->transition_pending = false;
                                }
                            });
                    }
                    catch (...)
                    {
                    }
                    if (!enqueued)
                    {
                        current->transition_pending = false;
                        event_args.Cancel(false);
                    }
                });
            entry->floating_window = window;
            window.AppWindow().Resize({
                entry->descriptor.floating_width,
                entry->descriptor.floating_height });
            window.Activate();
        }
        catch (...)
        {
            entry->closing_internally = true;
            try
            {
                window.Content(nullptr);
            }
            catch (...)
            {
            }
            try
            {
                RestoreDockedContent(*entry);
            }
            catch (...)
            {
            }
            try
            {
                window.Close();
            }
            catch (...)
            {
            }
            try
            {
                if (entry->closed_token.value != 0)
                {
                    window.Closed(entry->closed_token);
                }
            }
            catch (...)
            {
            }
            try
            {
                if (entry->app_closing_token.value != 0)
                {
                    window.AppWindow().Closing(entry->app_closing_token);
                }
            }
            catch (...)
            {
            }
            entry->floating_window = nullptr;
            entry->closed_token = {};
            entry->app_closing_token = {};
            entry->closing_internally = false;
        }
    }

    void DockablePanelController::Dock(winrt::hstring const& id) noexcept
    {
        auto* entry = Find(id);
        if (!entry || !entry->floating_window)
        {
            return;
        }

        auto const window = entry->floating_window;
        try
        {
            window.Content(nullptr);
            RestoreDockedContent(*entry);
        }
        catch (...)
        {
            [[maybe_unused]] auto const restored = RestoreFloatingContent(*entry, window);
            return;
        }

        entry->closing_internally = true;
        try
        {
            window.Close();
        }
        catch (...)
        {
            entry->closing_internally = false;
            [[maybe_unused]] auto const restored = RestoreFloatingContent(*entry, window);
            return;
        }

        try
        {
            window.Closed(entry->closed_token);
        }
        catch (...)
        {
        }
        try
        {
            window.AppWindow().Closing(entry->app_closing_token);
        }
        catch (...)
        {
        }
        entry->floating_window = nullptr;
        entry->closed_token = {};
        entry->app_closing_token = {};
        entry->closing_internally = false;
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
            auto const window = entry.floating_window;
            auto const closed_token = entry.closed_token;
            auto const app_closing_token = entry.app_closing_token;
            entry.floating_window = nullptr;
            entry.closed_token = {};
            entry.app_closing_token = {};
            entry.transition_pending = false;
            entry.closing_internally = true;

            if (!window)
            {
                continue;
            }

            try
            {
                window.Closed(closed_token);
            }
            catch (...)
            {
            }
            try
            {
                window.AppWindow().Closing(app_closing_token);
            }
            catch (...)
            {
            }
            try
            {
                window.Content(nullptr);
            }
            catch (...)
            {
            }
            try
            {
                window.Close();
            }
            catch (...)
            {
            }
        }
        entries_.clear();
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
        SetDockedButton(entry);
    }

    void DockablePanelController::FloatingWindowClosed(
        winrt::hstring const& id,
        winrt::Microsoft::UI::Xaml::Window const& window) noexcept
    {
        auto* entry = Find(id);
        if (!entry || shutting_down_ || entry->closing_internally ||
            entry->floating_window != window)
        {
            return;
        }

        try
        {
            window.Content(nullptr);
        }
        catch (...)
        {
        }
        entry->floating_window = nullptr;
        entry->closed_token = {};
        entry->app_closing_token = {};
        entry->transition_pending = false;
        try
        {
            entry->descriptor.dock_host.Content(entry->descriptor.panel);
        }
        catch (...)
        {
        }
        try
        {
            entry->descriptor.dock_column.Width(
                winrt::Microsoft::UI::Xaml::GridLength{ entry->descriptor.docked_width });
        }
        catch (...)
        {
        }
        SetDockedButton(*entry);
    }

    void DockablePanelController::SetFloatingButton(Entry& entry) noexcept
    {
        try
        {
            entry.descriptor.toggle_button.Content(winrt::box_value(L"Dock"));
            winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(
                entry.descriptor.toggle_button,
                PanelAutomationName(L"Dock", entry.descriptor.title));
        }
        catch (...)
        {
        }
    }

    void DockablePanelController::SetDockedButton(Entry& entry) noexcept
    {
        try
        {
            entry.descriptor.toggle_button.Content(winrt::box_value(L"Float"));
            winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(
                entry.descriptor.toggle_button,
                PanelAutomationName(L"Float", entry.descriptor.title));
        }
        catch (...)
        {
        }
    }

    bool DockablePanelController::RestoreFloatingContent(
        Entry& entry,
        winrt::Microsoft::UI::Xaml::Window const& window) noexcept
    {
        try
        {
            entry.descriptor.dock_host.Content(nullptr);
        }
        catch (...)
        {
        }
        try
        {
            entry.descriptor.dock_column.Width(winrt::Microsoft::UI::Xaml::GridLength{ 0.0 });
        }
        catch (...)
        {
        }

        try
        {
            window.Content(entry.descriptor.panel);
            SetFloatingButton(entry);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
}
