#include "pch.h"
#include "MainWindow.xaml.h"

#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <format>

namespace winrt::OctoPaint::WinUI::implementation
{
    void MainWindow::RootGrid_Loaded(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& event_args)
    {
        RefreshView();
    }

    void MainWindow::NewDocument_Click(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& event_args)
    {
        workspace_.NewDocument(
            std::format("Untitled {}", next_document_number_++),
            { .width = 1920, .height = 1080 });
        RefreshView();
    }

    void MainWindow::RefreshView()
    {
        auto const snapshot = workspace_.Snapshot();
        DocumentTitleText().Text(winrt::to_hstring(snapshot.document_title));
        StatusText().Text(winrt::to_hstring(snapshot.status_message));
    }
}

