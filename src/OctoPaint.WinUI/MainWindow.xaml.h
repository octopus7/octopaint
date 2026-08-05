#pragma once

#include "MainWindow.g.h"

#include <octopaint/application/Workspace.h>

namespace winrt::OctoPaint::WinUI::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow() = default;

        void RootGrid_Loaded(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& event_args);

        void NewDocument_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& event_args);

    private:
        void RefreshView();

        octopaint::application::Workspace workspace_;
        std::uint32_t next_document_number_{ 1 };
    };
}

namespace winrt::OctoPaint::WinUI::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}

