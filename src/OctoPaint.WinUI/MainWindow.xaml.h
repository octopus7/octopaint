#pragma once

#include "MainWindow.g.h"
#include "D3DCanvasRenderer.h"
#include "DockablePanelController.h"

#include <octopaint/application/EditorState.h>
#include <octopaint/application/Workspace.h>

#include <array>
#include <chrono>
#include <optional>

namespace winrt::OctoPaint::WinUI::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();
        ~MainWindow();

        void RootGrid_Loaded(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& event_args);

        void MainWindow_Closed(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::WindowEventArgs const& event_args);

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

        void SplashOverlay_Loaded(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& event_args);

        void RepositoryLink_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& event_args);

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

        void Canvas_PointerPressed(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& event_args);

        void Canvas_PointerMoved(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& event_args);

        void Canvas_PointerReleased(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& event_args);

        void Canvas_PointerCanceled(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& event_args);

        void Canvas_PointerCaptureLost(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& event_args);

        void Canvas_SizeChanged(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::SizeChangedEventArgs const& event_args);

        void DockPanelToggle_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& event_args);

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
        void AppendPointerSamples(Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& event_args);
        void CompletePaintStroke(bool commit);
        void RefreshCanvas(octopaint::application::WorkspaceSnapshot const& snapshot);
        void RefreshColorSwatches();
        [[nodiscard]] static octopaint::application::EditorTool ToolFromName(hstring const& tool_name) noexcept;
        [[nodiscard]] static hstring ToolName(octopaint::application::EditorTool tool);
        [[nodiscard]] static bool IsChecked(Microsoft::UI::Xaml::Controls::Primitives::ToggleButton const& toggle);

        void BeginColorEdit(
            bool foreground,
            Microsoft::UI::Xaml::FrameworkElement const& anchor);
        void SyncColorEditor();
        void UpdateColorPreview();
        void RestoreEditingColor();
        void FinishColorEdit(bool apply);
        void CloseColorEditorBeforeExternalChange();

        [[nodiscard]] static RgbaColor ToRgba(HsvaColor const& color) noexcept;
        [[nodiscard]] static HsvaColor ToHsva(RgbaColor const& color) noexcept;
        [[nodiscard]] static Microsoft::UI::Xaml::Media::SolidColorBrush ColorBrush(HsvaColor const& color);

        octopaint::application::Workspace workspace_;
        octopaint::application::EditorState editor_state_;
        octopaint::winui::D3DCanvasRenderer canvas_renderer_;
        octopaint::winui::DockablePanelController dockable_panels_;
        std::uint32_t next_document_number_{ 1 };
        std::vector<octopaint::application::DocumentId> tab_document_ids_;

        HsvaColor editing_color_{};
        HsvaColor original_edit_color_{};
        bool editing_foreground_{ true };
        bool color_edit_active_{};
        bool suppress_color_events_{};
        bool suppress_tab_events_{};
        bool suppress_tool_option_events_{};
        bool stylus_detected_{};
        bool paint_stroke_active_{};
        bool paint_segment_break_pending_{};
        bool dockable_panels_registered_{};
        std::uint32_t paint_pointer_id_{};
        std::uint64_t previous_pointer_timestamp_{};
        std::optional<octopaint::application::PaintStrokeRequest> active_paint_request_;
        Microsoft::UI::Xaml::DispatcherTimer splash_timer_{ nullptr };
    };
}

namespace winrt::OctoPaint::WinUI::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
