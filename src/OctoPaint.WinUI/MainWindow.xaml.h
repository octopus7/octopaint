#pragma once

#include "MainWindow.g.h"
#include "ToolOptionsState.h"

#include <octopaint/application/Workspace.h>

#include <array>

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

        void ToolButton_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& event_args);

        void ForegroundSwatch_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& event_args);

        void BackgroundSwatch_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& event_args);

        void SwapColors_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& event_args);

        void ResetColors_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& event_args);

        void ColorSlider_ValueChanged(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs const& event_args);

        void RgbNumberBox_ValueChanged(
            Microsoft::UI::Xaml::Controls::NumberBox const& sender,
            Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs const& event_args);

        void HexTextBox_TextChanged(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Controls::TextChangedEventArgs const& event_args);

        void ApplyColor_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& event_args);

        void CancelColor_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& event_args);

        void ColorEditorFlyout_Closed(
            Windows::Foundation::IInspectable const& sender,
            Windows::Foundation::IInspectable const& event_args);

        void DocumentTabs_SelectionChanged(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& event_args);

        void DocumentTabs_TabCloseRequested(
            Microsoft::UI::Xaml::Controls::TabView const& sender,
            Microsoft::UI::Xaml::Controls::TabViewTabCloseRequestedEventArgs const& event_args);


        void ToolOptionNumberBox_ValueChanged(
            Microsoft::UI::Xaml::Controls::NumberBox const& sender,
            Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs const& event_args);

        void ToolOptionToggle_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& event_args);

        fire_and_forget SensitivityButton_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& event_args);

        void SensitivityDialog_PrimaryButtonClick(
            Microsoft::UI::Xaml::Controls::ContentDialog const& sender,
            Microsoft::UI::Xaml::Controls::ContentDialogButtonClickEventArgs const& event_args);

        void Canvas_PointerInput(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& event_args);

    private:
        struct HsvaColor final
        {
            double hue{};
            double saturation{};
            double value{};
            double alpha{ 1.0 };
        };

        struct RgbaColor final
        {
            std::uint8_t red{};
            std::uint8_t green{};
            std::uint8_t blue{};
            std::uint8_t alpha{ 255 };
        };

        void RefreshView();
        void RefreshDocumentTabs(octopaint::application::WorkspaceSnapshot const& snapshot);
        void SelectTool(Microsoft::UI::Xaml::Controls::Primitives::ToggleButton const& selected_button);
        void ProjectToolOptionsToControls();
        void CaptureToolOptionsFromControls();
        void UpdatePointerDevice(Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& event_args);
        [[nodiscard]] static bool IsChecked(Microsoft::UI::Xaml::Controls::Primitives::ToggleButton const& toggle);

        void BeginColorEdit(
            bool foreground,
            Microsoft::UI::Xaml::FrameworkElement const& anchor);
        void SyncColorEditor();
        void UpdateColorPreview();
        void RestoreEditingColor();
        void FinishColorEdit(bool apply);
        void CloseColorEditorBeforeExternalChange();

        [[nodiscard]] HsvaColor& EditingTarget() noexcept;
        [[nodiscard]] HsvaColor const& EditingTarget() const noexcept;
        [[nodiscard]] static RgbaColor ToRgba(HsvaColor const& color) noexcept;
        [[nodiscard]] static HsvaColor ToHsva(RgbaColor const& color) noexcept;
        [[nodiscard]] static Microsoft::UI::Xaml::Media::SolidColorBrush ColorBrush(HsvaColor const& color);

        octopaint::application::Workspace workspace_;
        std::uint32_t next_document_number_{ 1 };
        std::vector<octopaint::application::DocumentId> tab_document_ids_;
        hstring selected_tool_{ L"Pencil" };

        HsvaColor foreground_color_{ 0.0, 0.0, 0.0, 1.0 };
        HsvaColor background_color_{ 0.0, 0.0, 1.0, 1.0 };
        HsvaColor original_edit_color_{};
        bool editing_foreground_{ true };
        bool color_edit_active_{};
        bool suppress_color_events_{};
        bool suppress_tab_events_{};
        bool suppress_tool_option_events_{};
        bool stylus_detected_{};
        octopaint::winui::ToolOptionsState tool_options_{};
    };
}

namespace winrt::OctoPaint::WinUI::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}

