#pragma once

#include "MainWindow.g.h"
#include "D3DCanvasRenderer.h"
#include "DockablePanelController.h"

#include <octopaint/application/EditorState.h>
#include <octopaint/application/Workspace.h>

#include <array>
#include <chrono>
#include <optional>
#include <vector>

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

        void Undo_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& event_args);

        void Redo_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& event_args);

        void AddRasterLayer_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& event_args);

        void AddGroupLayer_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& event_args);

        void DeleteLayer_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& event_args);

        void LayersList_SelectionChanged(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& event_args);

        void LayerOpacity_ValueChanged(
            Microsoft::UI::Xaml::Controls::NumberBox const& sender,
            Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs const& event_args);

        void ActiveLayerLock_Click(
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

        void SelectionCompleteAccelerator_Invoked(
            Microsoft::UI::Xaml::Input::KeyboardAccelerator const& sender,
            Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& event_args);

        void SelectionCancelAccelerator_Invoked(
            Microsoft::UI::Xaml::Input::KeyboardAccelerator const& sender,
            Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& event_args);

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
        void RefreshLayers(octopaint::application::WorkspaceSnapshot const& snapshot);
        void ToggleLayerVisibility(std::size_t layer_index);
        void SelectTool(Microsoft::UI::Xaml::Controls::Primitives::ToggleButton const& selected_button);
        void ProjectToolOptionsToControls();
        void CaptureToolOptionsFromControls();
        void UpdatePointerDevice(Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& event_args);
        void AppendPointerSamples(Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& event_args);
        void CompletePaintStroke(bool commit);
        [[nodiscard]] bool BeginSelectionGesture(
            octopaint::application::EditorTool tool,
            Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& event_args);
        void UpdateSelectionGesture(Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& event_args);
        void CompleteSelectionGesture(Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& event_args);
        void CancelSelectionGesture(bool clear_polygon) noexcept;
        [[nodiscard]] bool CompletePolygonSelection();
        [[nodiscard]] bool ApplySelectionGesture(octopaint::application::SelectionGestureRequest request);
        [[nodiscard]] std::optional<octopaint::application::SelectionPoint> TryMapSelectionPoint(
            Windows::Foundation::Point const& panel_point) const noexcept;
        void ProjectSelectionToRenderer(octopaint::application::WorkspaceSnapshot const& snapshot);
        void SetSelectionAnimationActive(bool active);
        void AdvanceSelectionAnimation();
        void RefreshCanvas(octopaint::application::WorkspaceSnapshot const& snapshot);
        void RefreshColorSwatches();
        [[nodiscard]] static octopaint::application::EditorTool ToolFromName(hstring const& tool_name) noexcept;
        [[nodiscard]] static hstring ToolName(octopaint::application::EditorTool tool);
        [[nodiscard]] static bool IsChecked(Microsoft::UI::Xaml::Controls::Primitives::ToggleButton const& toggle);
        [[nodiscard]] static bool IsSelectionTool(octopaint::application::EditorTool tool) noexcept;

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
        std::vector<octopaint::application::LayerId> layer_ids_;
        std::vector<bool> layer_visibilities_;

        HsvaColor editing_color_{};
        HsvaColor original_edit_color_{};
        bool editing_foreground_{ true };
        bool color_edit_active_{};
        bool suppress_color_events_{};
        bool suppress_tab_events_{};
        bool suppress_layer_events_{};
        bool suppress_tool_option_events_{};
        bool stylus_detected_{};
        bool paint_stroke_active_{};
        bool paint_segment_break_pending_{};
        bool selection_gesture_active_{};
        bool selection_outline_visible_{};
        bool selection_animation_timer_initialized_{};
        bool dockable_panels_registered_{};
        std::uint32_t paint_pointer_id_{};
        std::uint32_t selection_pointer_id_{};
        std::uint64_t previous_pointer_timestamp_{};
        octopaint::application::EditorTool selection_gesture_tool_{
            octopaint::application::EditorTool::RectangularMarquee };
        octopaint::application::SelectionPoint selection_anchor_{};
        octopaint::application::SelectionPoint selection_current_{};
        std::vector<octopaint::application::SelectionPoint> selection_path_;
        std::vector<octopaint::application::SelectionPoint> polygon_vertices_;
        std::optional<octopaint::application::DocumentId> selection_document_id_;
        std::optional<octopaint::application::SelectionPoint> previous_polygon_click_;
        std::chrono::steady_clock::time_point previous_polygon_click_time_{};
        std::optional<octopaint::application::PaintStrokeRequest> active_paint_request_;
        Microsoft::UI::Xaml::DispatcherTimer selection_animation_timer_{ nullptr };
        Microsoft::UI::Xaml::DispatcherTimer splash_timer_{ nullptr };
    };
}

namespace winrt::OctoPaint::WinUI::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
