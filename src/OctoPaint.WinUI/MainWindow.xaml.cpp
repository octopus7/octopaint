#include "pch.h"
#include "MainWindow.xaml.h"

#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <format>
#include <limits>
#include <ranges>
#include <stdexcept>

namespace winrt::OctoPaint::WinUI::implementation
{
    namespace
    {
        [[nodiscard]] std::uint8_t ToByte(double const value) noexcept
        {
            return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0, 1.0) * 255.0));
        }

        [[nodiscard]] double NumberBoxByte(Microsoft::UI::Xaml::Controls::NumberBox const& box) noexcept
        {
            auto const value = box.Value();
            return std::isnan(value) ? 0.0 : std::clamp(std::round(value), 0.0, 255.0);
        }
    }

    MainWindow::MainWindow()
    {
        std::array<wchar_t, MAX_PATH> executable_path{};
        auto const path_length = GetModuleFileNameW(
            nullptr,
            executable_path.data(),
            static_cast<DWORD>(executable_path.size()));

        if (path_length > 0 && path_length < executable_path.size())
        {
            auto const icon_path = std::filesystem::path(executable_path.data())
                .parent_path() / L"Assets" / L"OctoPaint.ico";
            AppWindow().SetIcon(winrt::hstring(icon_path.wstring()));
        }
    }

    MainWindow::~MainWindow()
    {
        if (selection_animation_timer_)
        {
            selection_animation_timer_.Stop();
        }
        dockable_panels_.Shutdown();
    }

    void MainWindow::MainWindow_Closed(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::WindowEventArgs const& event_args)
    {
        if (selection_animation_timer_)
        {
            selection_animation_timer_.Stop();
        }
        dockable_panels_.Shutdown();
    }

    void MainWindow::RootGrid_Loaded(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& event_args)
    {
        if (!dockable_panels_registered_)
        {
            dockable_panels_.Register({
                .id = L"tools",
                .title = L"Tools",
                .panel = ToolsPanel(),
                .dock_host = ToolsDockHost(),
                .toggle_button = ToolsDockToggleButton(),
                .dock_column = ToolsColumn(),
                .docked_width = 88.0,
                .floating_width = 180,
                .floating_height = 720 });
            dockable_panels_.Register({
                .id = L"layers",
                .title = L"Layers",
                .panel = LayersPanel(),
                .dock_host = LayersDockHost(),
                .toggle_button = LayersDockToggleButton(),
                .dock_column = LayersColumn(),
                .docked_width = 260.0,
                .floating_width = 320,
                .floating_height = 720 });
            dockable_panels_registered_ = true;
        }

        if (!selection_animation_timer_initialized_)
        {
            selection_animation_timer_ = Microsoft::UI::Xaml::DispatcherTimer{};
            selection_animation_timer_.Interval(std::chrono::milliseconds(100));
            selection_animation_timer_.Tick([weak_this = get_weak()](auto const&, auto const&)
            {
                if (auto const self = weak_this.get())
                {
                    self->AdvanceSelectionAnimation();
                }
            });
            selection_animation_timer_initialized_ = true;
        }

        canvas_renderer_.Attach(CanvasSwapChainPanel());
        RefreshColorSwatches();
        ProjectToolOptionsToControls();
        RefreshView();
    }

    void MainWindow::DockPanelToggle_Click(
        Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& event_args)
    {
        auto const button = sender.as<Microsoft::UI::Xaml::Controls::Button>();
        if (auto const tag = button.Tag())
        {
            CloseColorEditorBeforeExternalChange();
            auto const panel_id = unbox_value<hstring>(tag);
            dockable_panels_.RequestToggle(panel_id, DispatcherQueue());
        }
    }

    void MainWindow::SelectionCompleteAccelerator_Invoked(
        [[maybe_unused]] Microsoft::UI::Xaml::Input::KeyboardAccelerator const& sender,
        Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& event_args)
    {
        if (CompletePolygonSelection())
        {
            event_args.Handled(true);
        }
    }

    void MainWindow::SelectionCancelAccelerator_Invoked(
        [[maybe_unused]] Microsoft::UI::Xaml::Input::KeyboardAccelerator const& sender,
        Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs const& event_args)
    {
        if (selection_gesture_active_ || !polygon_vertices_.empty())
        {
            CancelSelectionGesture(true);
            event_args.Handled(true);
        }
    }

    void MainWindow::NewDocument_Click(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& event_args)
    {
        CancelSelectionGesture(true);
        workspace_.NewDocument(
            std::format("Untitled {}", next_document_number_++),
            { .width = 1920, .height = 1080 });
        RefreshView();
    }

    void MainWindow::ToolButton_Click(
        Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& event_args)
    {
        SelectTool(sender.as<Microsoft::UI::Xaml::Controls::Primitives::ToggleButton>());
        RefreshView();
    }

    void MainWindow::ForegroundSwatch_Click(
        Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& event_args)
    {
        BeginColorEdit(true, sender.as<Microsoft::UI::Xaml::FrameworkElement>());
    }

    void MainWindow::BackgroundSwatch_Click(
        Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& event_args)
    {
        BeginColorEdit(false, sender.as<Microsoft::UI::Xaml::FrameworkElement>());
    }

    void MainWindow::SwapColors_Click(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& event_args)
    {
        CloseColorEditorBeforeExternalChange();
        editor_state_.SwapColors();
        RefreshColorSwatches();
    }

    void MainWindow::ResetColors_Click(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& event_args)
    {
        CloseColorEditorBeforeExternalChange();
        editor_state_.ResetColors();
        RefreshColorSwatches();
    }

    void MainWindow::ColorSlider_ValueChanged(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs const& event_args)
    {
        if (suppress_color_events_ || !color_edit_active_)
        {
            return;
        }

        editing_color_.hue = HueSlider().Value();
        editing_color_.saturation = SaturationSlider().Value() / 100.0;
        editing_color_.value = ValueSlider().Value() / 100.0;
        editing_color_.alpha = AlphaSlider().Value() / 100.0;
        SyncColorEditor();
    }

    void MainWindow::RgbNumberBox_ValueChanged(
        [[maybe_unused]] Microsoft::UI::Xaml::Controls::NumberBox const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs const& event_args)
    {
        if (suppress_color_events_ || !color_edit_active_)
        {
            return;
        }

        auto const previous_alpha = editing_color_.alpha;
        editing_color_ = ToHsva({
            static_cast<std::uint8_t>(NumberBoxByte(RedNumberBox())),
            static_cast<std::uint8_t>(NumberBoxByte(GreenNumberBox())),
            static_cast<std::uint8_t>(NumberBoxByte(BlueNumberBox())),
            ToByte(previous_alpha) });
        editing_color_.alpha = previous_alpha;
        SyncColorEditor();
    }

    void MainWindow::HexTextBox_TextChanged(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::Controls::TextChangedEventArgs const& event_args)
    {
        if (suppress_color_events_ || !color_edit_active_)
        {
            return;
        }

        auto const text = winrt::to_string(HexTextBox().Text());
        if (text.size() != 7 || text.front() != '#')
        {
            return;
        }

        std::uint32_t rgb{};
        auto const result = std::from_chars(text.data() + 1, text.data() + text.size(), rgb, 16);
        if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
        {
            return;
        }

        auto const previous_alpha = editing_color_.alpha;
        editing_color_ = ToHsva({
            static_cast<std::uint8_t>((rgb >> 16) & 0xff),
            static_cast<std::uint8_t>((rgb >> 8) & 0xff),
            static_cast<std::uint8_t>(rgb & 0xff),
            ToByte(previous_alpha) });
        editing_color_.alpha = previous_alpha;
        SyncColorEditor();
    }

    void MainWindow::ApplyColor_Click(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& event_args)
    {
        FinishColorEdit(true);
    }

    void MainWindow::CancelColor_Click(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& event_args)
    {
        FinishColorEdit(false);
    }

    void MainWindow::ColorEditorFlyout_Closed(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Windows::Foundation::IInspectable const& event_args)
    {
        if (color_edit_active_)
        {
            RestoreEditingColor();
            color_edit_active_ = false;
        }
    }

    void MainWindow::DocumentTabs_SelectionChanged(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& event_args)
    {
        if (suppress_tab_events_)
        {
            return;
        }

        auto const selected_index = DocumentTabs().SelectedIndex();
        if (selected_index >= 0 && static_cast<std::size_t>(selected_index) < tab_document_ids_.size())
        {
            CancelSelectionGesture(true);
            if (workspace_.ActivateDocument(tab_document_ids_[static_cast<std::size_t>(selected_index)]))
            {
                RefreshView();
            }
        }
    }

    void MainWindow::DocumentTabs_TabCloseRequested(
        Microsoft::UI::Xaml::Controls::TabView const& sender,
        Microsoft::UI::Xaml::Controls::TabViewTabCloseRequestedEventArgs const& event_args)
    {
        std::uint32_t index{};
        if (sender.TabItems().IndexOf(event_args.Item(), index) && index < tab_document_ids_.size())
        {
            CancelSelectionGesture(true);
            [[maybe_unused]] auto const closed = workspace_.CloseDocument(tab_document_ids_[index]);
            RefreshView();
        }
    }

    void MainWindow::SplashOverlay_Loaded(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& event_args)
    {
        auto const animations_enabled = Windows::UI::ViewManagement::UISettings{}.AnimationsEnabled();
        if (animations_enabled)
        {
            SplashMotionStoryboard().Begin();
        }
        else
        {
            SplashBackgroundImage().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
            SplashCharacterImage().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
            SplashFallbackImage().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
        }

        splash_timer_ = Microsoft::UI::Xaml::DispatcherTimer{};
        splash_timer_.Interval(std::chrono::seconds(5));
        splash_timer_.Tick([weak_this = get_weak()](auto const&, auto const&)
        {
            if (auto const self = weak_this.get())
            {
                self->splash_timer_.Stop();
                self->SplashMotionStoryboard().Stop();
                self->SplashOverlay().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
            }
        });
        splash_timer_.Start();
    }

    void MainWindow::RepositoryLink_Click(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& event_args)
    {
        [[maybe_unused]] auto const launch_operation = Windows::System::Launcher::LaunchUriAsync(
            Windows::Foundation::Uri{ L"https://github.com/octopus7/octopaint" });
    }

    void MainWindow::ToolOptionNumberBox_ValueChanged(
        [[maybe_unused]] Microsoft::UI::Xaml::Controls::NumberBox const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs const& event_args)
    {
        CaptureToolOptionsFromControls();
    }

    void MainWindow::ToolOptionToggle_Click(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& event_args)
    {
        CaptureToolOptionsFromControls();
    }

    fire_and_forget MainWindow::SensitivityButton_Click(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& event_args)
    {
        auto lifetime = get_strong();
        auto const state = editor_state_.Snapshot();
        auto const gamma = state.Pressure().gamma;
        auto preset = 3;
        if (std::abs(gamma - 1.0F) < 0.001F)
        {
            preset = 0;
        }
        else if (std::abs(gamma - 0.65F) < 0.001F)
        {
            preset = 1;
        }
        else if (std::abs(gamma - 1.5F) < 0.001F)
        {
            preset = 2;
        }
        PressureCurveComboBox().SelectedIndex(preset);
        PressureGammaNumberBox().Value(gamma);
        StabilizerStrengthNumberBox().Value(state.Stabilizer().strength * 100.0);
        StabilizerSmoothingNumberBox().Value(state.Stabilizer().smoothing * 100.0);
        SensitivityDialog().XamlRoot(RootGrid().XamlRoot());
        [[maybe_unused]] auto const result = co_await SensitivityDialog().ShowAsync();
    }

    void MainWindow::SensitivityDialog_PrimaryButtonClick(
        [[maybe_unused]] Microsoft::UI::Xaml::Controls::ContentDialog const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::Controls::ContentDialogButtonClickEventArgs const& event_args)
    {
        auto gamma = PressureGammaNumberBox().Value();
        switch (PressureCurveComboBox().SelectedIndex())
        {
        case 0: gamma = 1.0; break;
        case 1: gamma = 0.65; break;
        case 2: gamma = 1.5; break;
        default: break;
        }
        editor_state_.SetPressureGamma(static_cast<float>(gamma));
        editor_state_.SetStabilizerStrength(
            static_cast<float>(StabilizerStrengthNumberBox().Value() / 100.0));
        editor_state_.SetStabilizerSmoothing(
            static_cast<float>(StabilizerSmoothingNumberBox().Value() / 100.0));
    }

    void MainWindow::Canvas_PointerPressed(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& event_args)
    {
        UpdatePointerDevice(event_args);
        if (paint_stroke_active_ || selection_gesture_active_)
        {
            return;
        }

        auto const state = editor_state_.Snapshot();
        auto const tool = state.ActiveTool();
        if (IsSelectionTool(tool))
        {
            if (BeginSelectionGesture(tool, event_args))
            {
                event_args.Handled(true);
            }
            return;
        }
        if (tool != octopaint::application::EditorTool::Pencil &&
            tool != octopaint::application::EditorTool::Airbrush)
        {
            return;
        }

        auto const point = event_args.GetCurrentPoint(CanvasInputSurface());
        auto const properties = point.Properties();
        if (!properties.IsLeftButtonPressed() && !point.IsInContact())
        {
            return;
        }

        auto const snapshot = workspace_.Snapshot();
        if (!snapshot.active_document_id || !snapshot.active_layer_id ||
            !canvas_renderer_.TryMapPanelToDocument(
                static_cast<float>(point.Position().X),
                static_cast<float>(point.Position().Y)))
        {
            return;
        }

        auto const color = state.ForegroundColor();
        auto const& brush = state.Brush();
        auto const& pressure = state.Pressure();
        auto const& stabilizer = state.Stabilizer();
        active_paint_request_.emplace(octopaint::application::PaintStrokeRequest{
            .document_id = *snapshot.active_document_id,
            .layer_id = *snapshot.active_layer_id,
            .tool = tool == octopaint::application::EditorTool::Airbrush
                ? octopaint::application::PaintTool::Airbrush
                : octopaint::application::PaintTool::Pencil,
            .color = { color.red, color.green, color.blue, color.alpha },
            .brush = {
                .radius = brush.size_pixels * 0.5F,
                .flow_per_second = brush.flow,
                .fixed_timestep_seconds = 1.0F / 60.0F,
                .hardness = brush.hardness,
                .spacing = brush.spacing,
                .opacity = brush.opacity,
                .pressure_affects_size = brush.pressure_affects_size,
                .pressure_affects_opacity = brush.pressure_affects_opacity },
            .pressure = {
                .minimum_input = pressure.minimum_input,
                .maximum_input = pressure.maximum_input,
                .gamma = pressure.gamma },
            .stabilizer = {
                .enabled = stabilizer.enabled,
                .strength = stabilizer.strength }
        });

        try
        {
            paint_stroke_active_ = CanvasInputSurface().CapturePointer(event_args.Pointer());
        }
        catch (...)
        {
            active_paint_request_.reset();
            paint_stroke_active_ = false;
            return;
        }
        if (!paint_stroke_active_)
        {
            active_paint_request_.reset();
            return;
        }

        paint_pointer_id_ = event_args.Pointer().PointerId();
        previous_pointer_timestamp_ = 0;
        paint_segment_break_pending_ = false;
        AppendPointerSamples(event_args);
        event_args.Handled(true);
    }

    void MainWindow::Canvas_PointerMoved(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& event_args)
    {
        UpdatePointerDevice(event_args);
        if (selection_gesture_active_ && event_args.Pointer().PointerId() == selection_pointer_id_)
        {
            UpdateSelectionGesture(event_args);
            event_args.Handled(true);
            return;
        }
        if (!paint_stroke_active_ || event_args.Pointer().PointerId() != paint_pointer_id_)
        {
            return;
        }

        AppendPointerSamples(event_args);
        event_args.Handled(true);
    }

    void MainWindow::Canvas_PointerReleased(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& event_args)
    {
        if (selection_gesture_active_ && event_args.Pointer().PointerId() == selection_pointer_id_)
        {
            CompleteSelectionGesture(event_args);
            event_args.Handled(true);
            return;
        }
        if (!paint_stroke_active_ || event_args.Pointer().PointerId() != paint_pointer_id_)
        {
            return;
        }

        AppendPointerSamples(event_args);
        paint_stroke_active_ = false;
        event_args.Handled(true);
        try
        {
            CanvasInputSurface().ReleasePointerCapture(event_args.Pointer());
        }
        catch (...)
        {
            try
            {
                CanvasInputSurface().ReleasePointerCaptures();
            }
            catch (...)
            {
            }
        }
        CompletePaintStroke(true);
    }

    void MainWindow::Canvas_PointerCanceled(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& event_args)
    {
        if (selection_gesture_active_ && event_args.Pointer().PointerId() == selection_pointer_id_)
        {
            CancelSelectionGesture(false);
            event_args.Handled(true);
            return;
        }
        if (paint_stroke_active_ && event_args.Pointer().PointerId() == paint_pointer_id_)
        {
            paint_stroke_active_ = false;
            event_args.Handled(true);
            try
            {
                CanvasInputSurface().ReleasePointerCapture(event_args.Pointer());
            }
            catch (...)
            {
                try
                {
                    CanvasInputSurface().ReleasePointerCaptures();
                }
                catch (...)
                {
                }
            }
            CompletePaintStroke(false);
        }
    }

    void MainWindow::Canvas_PointerCaptureLost(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& event_args)
    {
        if (selection_gesture_active_ && event_args.Pointer().PointerId() == selection_pointer_id_)
        {
            CancelSelectionGesture(false);
            return;
        }
        if (paint_stroke_active_ && event_args.Pointer().PointerId() == paint_pointer_id_)
        {
            paint_stroke_active_ = false;
            CompletePaintStroke(true);
        }
    }

    void MainWindow::Canvas_SizeChanged(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::SizeChangedEventArgs const& event_args)
    {
        auto scale = 1.0F;
        if (auto const xaml_root = CanvasInputSurface().XamlRoot())
        {
            scale = static_cast<float>(xaml_root.RasterizationScale());
        }
        canvas_renderer_.Resize(
            static_cast<float>(event_args.NewSize().Width),
            static_cast<float>(event_args.NewSize().Height),
            scale);
        [[maybe_unused]] auto const rendered = canvas_renderer_.Render();
    }

    void MainWindow::AppendPointerSamples(
        Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& event_args)
    {
        if (!active_paint_request_)
        {
            return;
        }

        std::vector<Microsoft::UI::Input::PointerPoint> points;
        for (auto const& point : event_args.GetIntermediatePoints(CanvasInputSurface()))
        {
            points.push_back(point);
        }
        std::ranges::sort(points, [](auto const& left, auto const& right)
        {
            return left.Timestamp() < right.Timestamp();
        });

        for (auto const& point : points)
        {
            auto const timestamp = point.Timestamp();
            if (previous_pointer_timestamp_ != 0 && timestamp <= previous_pointer_timestamp_)
            {
                continue;
            }

            auto const document_point = canvas_renderer_.TryMapPanelToDocument(
                static_cast<float>(point.Position().X),
                static_cast<float>(point.Position().Y));
            if (!document_point)
            {
                paint_segment_break_pending_ = !active_paint_request_->samples.empty();
                previous_pointer_timestamp_ = timestamp;
                continue;
            }

            auto pressure = 1.0F;
            if (event_args.Pointer().PointerDeviceType() == Microsoft::UI::Input::PointerDeviceType::Pen)
            {
                pressure = std::clamp(point.Properties().Pressure(), 0.0F, 1.0F);
            }

            auto const elapsed = previous_pointer_timestamp_ == 0 || paint_segment_break_pending_
                ? 0.0
                : static_cast<double>(timestamp - previous_pointer_timestamp_) / 1'000'000.0;
            active_paint_request_->samples.push_back({
                .x = static_cast<double>(document_point->x),
                .y = static_cast<double>(document_point->y),
                .pressure = pressure,
                .elapsed_seconds = elapsed,
                .begins_new_segment = paint_segment_break_pending_ });
            previous_pointer_timestamp_ = timestamp;
            paint_segment_break_pending_ = false;
        }
    }

    void MainWindow::CompletePaintStroke(bool const commit)
    {
        paint_stroke_active_ = false;
        auto request = std::move(active_paint_request_);
        active_paint_request_.reset();
        paint_pointer_id_ = 0;
        previous_pointer_timestamp_ = 0;
        paint_segment_break_pending_ = false;

        if (commit && request && !request->samples.empty())
        {
            [[maybe_unused]] auto const result = workspace_.ApplyPaintStroke(*request);
            RefreshView();
        }
    }

    bool MainWindow::BeginSelectionGesture(
        octopaint::application::EditorTool const tool,
        Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& event_args)
    {
        auto const pointer_point = event_args.GetCurrentPoint(CanvasInputSurface());
        auto const properties = pointer_point.Properties();
        if (!properties.IsLeftButtonPressed() && !pointer_point.IsInContact())
        {
            return false;
        }

        auto const document_point = TryMapSelectionPoint(pointer_point.Position());
        auto const snapshot = workspace_.Snapshot();
        if (!document_point || !snapshot.active_document_id)
        {
            return false;
        }

        if (tool != octopaint::application::EditorTool::PolygonalLasso)
        {
            polygon_vertices_.clear();
            previous_polygon_click_.reset();
        }

        try
        {
            if (!CanvasInputSurface().CapturePointer(event_args.Pointer()))
            {
                return false;
            }
        }
        catch (...)
        {
            return false;
        }

        selection_gesture_active_ = true;
        selection_pointer_id_ = event_args.Pointer().PointerId();
        selection_gesture_tool_ = tool;
        selection_document_id_ = snapshot.active_document_id;
        selection_anchor_ = *document_point;
        selection_current_ = *document_point;
        selection_path_.clear();
        if (tool == octopaint::application::EditorTool::FreehandLasso)
        {
            selection_path_.push_back(*document_point);
        }
        return true;
    }

    void MainWindow::UpdateSelectionGesture(
        Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& event_args)
    {
        if (!selection_gesture_active_)
        {
            return;
        }

        if (selection_gesture_tool_ != octopaint::application::EditorTool::FreehandLasso)
        {
            auto const pointer_point = event_args.GetCurrentPoint(CanvasInputSurface());
            if (auto const document_point = TryMapSelectionPoint(pointer_point.Position()))
            {
                selection_current_ = *document_point;
            }
            return;
        }

        std::vector<Microsoft::UI::Input::PointerPoint> pointer_points;
        for (auto const& point : event_args.GetIntermediatePoints(CanvasInputSurface()))
        {
            pointer_points.push_back(point);
        }
        std::ranges::sort(pointer_points, [](auto const& left, auto const& right)
        {
            return left.Timestamp() < right.Timestamp();
        });

        for (auto const& point : pointer_points)
        {
            auto const document_point = TryMapSelectionPoint(point.Position());
            if (!document_point)
            {
                continue;
            }
            selection_current_ = *document_point;
            if (selection_path_.empty() || selection_path_.back() != *document_point)
            {
                selection_path_.push_back(*document_point);
            }
        }
    }

    void MainWindow::CompleteSelectionGesture(
        Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& event_args)
    {
        UpdateSelectionGesture(event_args);

        auto const tool = selection_gesture_tool_;
        auto const document_id = selection_document_id_;
        auto const anchor = selection_anchor_;
        auto const current = selection_current_;
        selection_gesture_active_ = false;
        selection_pointer_id_ = 0;
        selection_document_id_.reset();

        try
        {
            CanvasInputSurface().ReleasePointerCapture(event_args.Pointer());
        }
        catch (...)
        {
            try
            {
                CanvasInputSurface().ReleasePointerCaptures();
            }
            catch (...)
            {
            }
        }

        if (!document_id)
        {
            selection_path_.clear();
            return;
        }

        if (tool == octopaint::application::EditorTool::PolygonalLasso)
        {
            constexpr std::int64_t FinishDistanceSquared = 16;
            auto const distance_squared = [](auto const& left, auto const& right)
            {
                auto const dx = static_cast<std::int64_t>(left.x) - right.x;
                auto const dy = static_cast<std::int64_t>(left.y) - right.y;
                return dx * dx + dy * dy;
            };
            auto const now = std::chrono::steady_clock::now();
            auto const closes_path = polygon_vertices_.size() >= 3 &&
                distance_squared(current, polygon_vertices_.front()) <= FinishDistanceSquared;
            auto const is_double_click = polygon_vertices_.size() >= 3 && previous_polygon_click_ &&
                now - previous_polygon_click_time_ <= std::chrono::milliseconds(500) &&
                distance_squared(current, *previous_polygon_click_) <= FinishDistanceSquared;

            if (closes_path || is_double_click)
            {
                [[maybe_unused]] auto const completed = CompletePolygonSelection();
                return;
            }

            if (polygon_vertices_.empty() || polygon_vertices_.back() != current)
            {
                polygon_vertices_.push_back(current);
            }
            previous_polygon_click_ = current;
            previous_polygon_click_time_ = now;
            return;
        }

        octopaint::application::SelectionGestureRequest request{
            .document_id = *document_id
        };
        if (tool == octopaint::application::EditorTool::FreehandLasso)
        {
            request.kind = octopaint::application::SelectionGestureKind::Freehand;
            request.points = std::move(selection_path_);
        }
        else
        {
            auto const left = (std::min)(anchor.x, current.x);
            auto const top = (std::min)(anchor.y, current.y);
            auto const width = static_cast<std::int64_t>((std::max)(anchor.x, current.x)) - left + 1;
            auto const height = static_cast<std::int64_t>((std::max)(anchor.y, current.y)) - top + 1;
            if (width > (std::numeric_limits<std::int32_t>::max)() ||
                height > (std::numeric_limits<std::int32_t>::max)())
            {
                selection_path_.clear();
                return;
            }
            request.kind = tool == octopaint::application::EditorTool::EllipticalMarquee
                ? octopaint::application::SelectionGestureKind::Elliptical
                : octopaint::application::SelectionGestureKind::Rectangular;
            request.bounds = {
                .x = left,
                .y = top,
                .width = static_cast<std::int32_t>(width),
                .height = static_cast<std::int32_t>(height) };
        }
        [[maybe_unused]] auto const applied = ApplySelectionGesture(std::move(request));
        selection_path_.clear();
    }

    void MainWindow::CancelSelectionGesture(bool const clear_polygon) noexcept
    {
        selection_gesture_active_ = false;
        selection_pointer_id_ = 0;
        selection_document_id_.reset();
        selection_path_.clear();
        if (clear_polygon)
        {
            polygon_vertices_.clear();
            previous_polygon_click_.reset();
            previous_polygon_click_time_ = {};
        }
        try
        {
            CanvasInputSurface().ReleasePointerCaptures();
        }
        catch (...)
        {
        }
    }

    bool MainWindow::CompletePolygonSelection()
    {
        if (selection_gesture_active_ || polygon_vertices_.size() < 3)
        {
            return false;
        }

        auto const snapshot = workspace_.Snapshot();
        if (!snapshot.active_document_id)
        {
            CancelSelectionGesture(true);
            return false;
        }

        auto request = octopaint::application::SelectionGestureRequest{
            .document_id = *snapshot.active_document_id,
            .kind = octopaint::application::SelectionGestureKind::Polygonal,
            .points = std::move(polygon_vertices_) };
        previous_polygon_click_.reset();
        previous_polygon_click_time_ = {};
        [[maybe_unused]] auto const applied = ApplySelectionGesture(std::move(request));
        polygon_vertices_.clear();
        return true;
    }

    bool MainWindow::ApplySelectionGesture(octopaint::application::SelectionGestureRequest request)
    {
        try
        {
            auto const result = workspace_.ApplySelectionGesture(request);
            RefreshView();
            return result.status == octopaint::application::SelectionStatus::Applied ||
                result.status == octopaint::application::SelectionStatus::NoChange;
        }
        catch (...)
        {
            return false;
        }
    }

    std::optional<octopaint::application::SelectionPoint> MainWindow::TryMapSelectionPoint(
        Windows::Foundation::Point const& panel_point) const noexcept
    {
        auto const point = canvas_renderer_.TryMapPanelToDocument(panel_point.X, panel_point.Y);
        if (!point || point->x > static_cast<std::uint32_t>((std::numeric_limits<std::int32_t>::max)()) ||
            point->y > static_cast<std::uint32_t>((std::numeric_limits<std::int32_t>::max)()))
        {
            return std::nullopt;
        }
        return octopaint::application::SelectionPoint{
            .x = static_cast<std::int32_t>(point->x),
            .y = static_cast<std::int32_t>(point->y) };
    }

    void MainWindow::ProjectSelectionToRenderer(
        octopaint::application::WorkspaceSnapshot const& snapshot)
    {
        if (!snapshot.active_document_id)
        {
            canvas_renderer_.ClearSelectionOutline();
            SetSelectionAnimationActive(false);
            return;
        }

        auto const boundary = workspace_.SnapshotSelectionBoundary(*snapshot.active_document_id);
        if (!boundary || !boundary->has_selection || boundary->edges.empty())
        {
            canvas_renderer_.ClearSelectionOutline();
            SetSelectionAnimationActive(false);
            return;
        }

        std::vector<octopaint::winui::SelectionEdgeSegment> segments;
        segments.reserve(boundary->edges.size());
        for (auto const& edge : boundary->edges)
        {
            segments.push_back({
                .x1 = static_cast<float>(edge.from.x),
                .y1 = static_cast<float>(edge.from.y),
                .x2 = static_cast<float>(edge.to.x),
                .y2 = static_cast<float>(edge.to.y) });
        }

        try
        {
            canvas_renderer_.SetSelectionOutline(segments);
            SetSelectionAnimationActive(true);
        }
        catch (...)
        {
            canvas_renderer_.ClearSelectionOutline();
            SetSelectionAnimationActive(false);
        }
    }

    void MainWindow::SetSelectionAnimationActive(bool const active)
    {
        selection_outline_visible_ = active;
        if (!selection_animation_timer_initialized_ || !selection_animation_timer_)
        {
            return;
        }
        if (active)
        {
            selection_animation_timer_.Start();
        }
        else
        {
            selection_animation_timer_.Stop();
            canvas_renderer_.SetSelectionAnimationPhase(0.0F);
        }
    }

    void MainWindow::AdvanceSelectionAnimation()
    {
        if (!selection_outline_visible_)
        {
            SetSelectionAnimationActive(false);
            return;
        }
        try
        {
            canvas_renderer_.AdvanceSelectionAnimationPhase(1.0F);
            [[maybe_unused]] auto const rendered = canvas_renderer_.Render();
        }
        catch (...)
        {
            canvas_renderer_.ClearSelectionOutline();
            SetSelectionAnimationActive(false);
        }
    }

    void MainWindow::ProjectToolOptionsToControls()
    {
        auto const state = editor_state_.Snapshot();
        auto const& brush = state.Brush();
        suppress_tool_option_events_ = true;
        BrushSizeNumberBox().Value(brush.size_pixels);
        HardnessNumberBox().Value(brush.hardness * 100.0);
        SpacingNumberBox().Value(brush.spacing * 100.0);
        FlowNumberBox().Value(brush.flow * 100.0);
        OpacityNumberBox().Value(brush.opacity * 100.0);
        PressureSizeCheckBox().IsChecked(brush.pressure_affects_size);
        PressureOpacityCheckBox().IsChecked(brush.pressure_affects_opacity);
        StrokeStabilizerCheckBox().IsChecked(state.Stabilizer().enabled);
        PressureSizeCheckBox().IsEnabled(stylus_detected_);
        PressureOpacityCheckBox().IsEnabled(stylus_detected_);
        suppress_tool_option_events_ = false;
    }

    void MainWindow::CaptureToolOptionsFromControls()
    {
        if (suppress_tool_option_events_)
        {
            return;
        }

        auto const value_or = [](Microsoft::UI::Xaml::Controls::NumberBox const& box, double const fallback)
        {
            auto const value = box.Value();
            return std::isnan(value) ? fallback : value;
        };

        auto const state = editor_state_.Snapshot();
        editor_state_.SetBrushSize(static_cast<float>(
            value_or(BrushSizeNumberBox(), state.Brush().size_pixels)));
        editor_state_.SetBrushHardness(static_cast<float>(
            value_or(HardnessNumberBox(), state.Brush().hardness * 100.0) / 100.0));
        editor_state_.SetBrushSpacing(static_cast<float>(
            value_or(SpacingNumberBox(), state.Brush().spacing * 100.0) / 100.0));
        editor_state_.SetBrushFlow(static_cast<float>(
            value_or(FlowNumberBox(), state.Brush().flow * 100.0) / 100.0));
        editor_state_.SetBrushOpacity(static_cast<float>(
            value_or(OpacityNumberBox(), state.Brush().opacity * 100.0) / 100.0));
        editor_state_.SetPressureAffectsSize(IsChecked(PressureSizeCheckBox()));
        editor_state_.SetPressureAffectsOpacity(IsChecked(PressureOpacityCheckBox()));
        editor_state_.SetStabilizerEnabled(IsChecked(StrokeStabilizerCheckBox()));
    }

    void MainWindow::UpdatePointerDevice(Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& event_args)
    {
        auto const device_type = event_args.Pointer().PointerDeviceType();
        if (device_type == Microsoft::UI::Input::PointerDeviceType::Pen)
        {
            if (!stylus_detected_)
            {
                stylus_detected_ = true;
                PressureSizeCheckBox().IsEnabled(true);
                PressureOpacityCheckBox().IsEnabled(true);
            }
            StylusStatusText().Text(L"Stylus detected — pressure controls enabled");
            return;
        }

        if (!stylus_detected_)
        {
            StylusStatusText().Text(
                device_type == Microsoft::UI::Input::PointerDeviceType::Touch
                    ? L"Touch input — stylus not detected"
                    : L"Mouse input — stylus not detected");
        }
    }

    bool MainWindow::IsChecked(Microsoft::UI::Xaml::Controls::Primitives::ToggleButton const& toggle)
    {
        auto const checked = toggle.IsChecked();
        return checked && checked.Value();
    }

    bool MainWindow::IsSelectionTool(octopaint::application::EditorTool const tool) noexcept
    {
        return tool == octopaint::application::EditorTool::RectangularMarquee ||
            tool == octopaint::application::EditorTool::EllipticalMarquee ||
            tool == octopaint::application::EditorTool::FreehandLasso ||
            tool == octopaint::application::EditorTool::PolygonalLasso;
    }

    void MainWindow::RefreshView()
    {
        auto const snapshot = workspace_.Snapshot();
        DocumentTitleText().Text(winrt::to_hstring(snapshot.document_title));
        StatusText().Text(winrt::to_hstring(std::format(
            "{} | Tool: {}",
            snapshot.status_message,
            winrt::to_string(ToolName(editor_state_.Snapshot().ActiveTool())))));
        RefreshDocumentTabs(snapshot);
        RefreshCanvas(snapshot);
    }

    void MainWindow::RefreshCanvas(octopaint::application::WorkspaceSnapshot const& snapshot)
    {
        if (!snapshot.active_document_id || !snapshot.active_layer_id)
        {
            canvas_renderer_.ClearDocument();
            ProjectSelectionToRenderer(snapshot);
            [[maybe_unused]] auto const rendered = canvas_renderer_.Render();
            DocumentTitleText().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
            return;
        }

        auto const pixels = workspace_.SnapshotRasterLayerPixels(
            *snapshot.active_document_id,
            *snapshot.active_layer_id);
        if (!pixels)
        {
            canvas_renderer_.ClearDocument();
            ProjectSelectionToRenderer(snapshot);
            [[maybe_unused]] auto const rendered = canvas_renderer_.Render();
            DocumentTitleText().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
            return;
        }

        DocumentTitleText().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
        if (pixels->row_stride > (std::numeric_limits<std::uint32_t>::max)())
        {
            throw std::overflow_error("The raster snapshot stride exceeds the D3D canvas pitch range.");
        }
        canvas_renderer_.SetDocument({
            .width = pixels->size.width,
            .height = pixels->size.height,
            .stride = static_cast<std::uint32_t>(pixels->row_stride),
            .premultiplied_bgra = pixels->pixels_bgra_premultiplied });
        ProjectSelectionToRenderer(snapshot);
        [[maybe_unused]] auto const rendered = canvas_renderer_.Render();
    }

    octopaint::application::EditorTool MainWindow::ToolFromName(hstring const& tool_name) noexcept
    {
        if (tool_name == L"Airbrush") return octopaint::application::EditorTool::Airbrush;
        if (tool_name == L"Rectangular Marquee") return octopaint::application::EditorTool::RectangularMarquee;
        if (tool_name == L"Elliptical Marquee") return octopaint::application::EditorTool::EllipticalMarquee;
        if (tool_name == L"Freehand Lasso") return octopaint::application::EditorTool::FreehandLasso;
        if (tool_name == L"Polygonal Lasso") return octopaint::application::EditorTool::PolygonalLasso;
        if (tool_name == L"Move Layer") return octopaint::application::EditorTool::MoveLayer;
        return octopaint::application::EditorTool::Pencil;
    }

    hstring MainWindow::ToolName(octopaint::application::EditorTool const tool)
    {
        switch (tool)
        {
        case octopaint::application::EditorTool::Airbrush: return L"Airbrush";
        case octopaint::application::EditorTool::RectangularMarquee: return L"Rectangular Marquee";
        case octopaint::application::EditorTool::EllipticalMarquee: return L"Elliptical Marquee";
        case octopaint::application::EditorTool::FreehandLasso: return L"Freehand Lasso";
        case octopaint::application::EditorTool::PolygonalLasso: return L"Polygonal Lasso";
        case octopaint::application::EditorTool::MoveLayer: return L"Move Layer";
        default: return L"Pencil";
        }
    }

    void MainWindow::RefreshDocumentTabs(octopaint::application::WorkspaceSnapshot const& snapshot)
    {
        suppress_tab_events_ = true;

        auto const items = DocumentTabs().TabItems();
        items.Clear();
        tab_document_ids_.clear();

        std::int32_t active_index = -1;
        for (auto const& document : snapshot.documents)
        {
            Microsoft::UI::Xaml::Controls::TabViewItem tab;
            tab.Header(box_value(winrt::to_hstring(document.title + (document.is_dirty ? " *" : ""))));
            tab.IsClosable(true);
            Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(
                tab,
                winrt::to_hstring(std::format("Document {}", document.title)));

            items.Append(tab);
            tab_document_ids_.push_back(document.id);
            if (document.is_active)
            {
                active_index = static_cast<std::int32_t>(tab_document_ids_.size() - 1);
            }
        }

        DocumentTabs().SelectedIndex(active_index);
        suppress_tab_events_ = false;
    }

    void MainWindow::SelectTool(Microsoft::UI::Xaml::Controls::Primitives::ToggleButton const& selected_button)
    {
        auto const selected_tool = ToolFromName(unbox_value<hstring>(selected_button.Tag()));
        if (editor_state_.Snapshot().ActiveTool() != selected_tool)
        {
            if (paint_stroke_active_)
            {
                CompletePaintStroke(false);
            }
            CancelSelectionGesture(true);
        }

        auto const buttons = std::array{
            PencilToolButton(),
            AirbrushToolButton(),
            RectangularMarqueeToolButton(),
            EllipticalMarqueeToolButton(),
            FreehandLassoToolButton(),
            PolygonalLassoToolButton(),
            MoveLayerToolButton() };

        for (auto const& button : buttons)
        {
            button.IsChecked(button == selected_button);
        }

        editor_state_.SetActiveTool(selected_tool);
    }

    void MainWindow::BeginColorEdit(
        bool const foreground,
        Microsoft::UI::Xaml::FrameworkElement const& anchor)
    {
        CloseColorEditorBeforeExternalChange();
        editing_foreground_ = foreground;
        auto const state = editor_state_.Snapshot();
        auto const source = foreground ? state.ForegroundColor() : state.BackgroundColor();
        editing_color_ = ToHsva({ source.red, source.green, source.blue, source.alpha });
        original_edit_color_ = editing_color_;
        color_edit_active_ = true;
        ColorEditorTitle().Text(foreground ? L"Foreground color" : L"Background color");
        SyncColorEditor();
        ColorEditorFlyout().ShowAt(anchor);
    }

    void MainWindow::SyncColorEditor()
    {
        if (!color_edit_active_)
        {
            return;
        }

        suppress_color_events_ = true;

        auto const& color = editing_color_;
        auto const rgba = ToRgba(color);

        HueSlider().Value(color.hue);
        SaturationSlider().Value(color.saturation * 100.0);
        ValueSlider().Value(color.value * 100.0);
        AlphaSlider().Value(color.alpha * 100.0);
        HueValueText().Text(winrt::to_hstring(std::format("{:.0f}°", color.hue)));
        SaturationValueText().Text(winrt::to_hstring(std::format("{:.0f}%", color.saturation * 100.0)));
        ValueValueText().Text(winrt::to_hstring(std::format("{:.0f}%", color.value * 100.0)));
        AlphaValueText().Text(winrt::to_hstring(std::format("{:.0f}%", color.alpha * 100.0)));

        RedNumberBox().Value(rgba.red);
        GreenNumberBox().Value(rgba.green);
        BlueNumberBox().Value(rgba.blue);
        HexTextBox().Text(winrt::to_hstring(std::format("#{:02X}{:02X}{:02X}", rgba.red, rgba.green, rgba.blue)));
        UpdateColorPreview();
        suppress_color_events_ = false;
    }

    void MainWindow::UpdateColorPreview()
    {
        auto const brush = ColorBrush(editing_color_);
        ColorEditorPreview().Background(brush);
        if (editing_foreground_)
        {
            ForegroundSwatchPreview().Background(brush);
        }
        else
        {
            BackgroundSwatchPreview().Background(brush);
        }
    }

    void MainWindow::RestoreEditingColor()
    {
        editing_color_ = original_edit_color_;
        RefreshColorSwatches();
    }

    void MainWindow::FinishColorEdit(bool const apply)
    {
        if (!color_edit_active_)
        {
            return;
        }

        if (apply)
        {
            auto const color = ToRgba(editing_color_);
            auto const editor_color = octopaint::application::EditorColor{
                color.red, color.green, color.blue, color.alpha };
            if (editing_foreground_)
            {
                editor_state_.SetForegroundColor(editor_color);
            }
            else
            {
                editor_state_.SetBackgroundColor(editor_color);
            }
            RefreshColorSwatches();
        }
        else
        {
            RestoreEditingColor();
        }

        color_edit_active_ = false;
        ColorEditorFlyout().Hide();
    }

    void MainWindow::CloseColorEditorBeforeExternalChange()
    {
        if (color_edit_active_)
        {
            RestoreEditingColor();
            color_edit_active_ = false;
            ColorEditorFlyout().Hide();
        }
    }

    void MainWindow::RefreshColorSwatches()
    {
        auto const state = editor_state_.Snapshot();
        auto const foreground = state.ForegroundColor();
        auto const background = state.BackgroundColor();
        ForegroundSwatchPreview().Background(ColorBrush(ToHsva({
            foreground.red, foreground.green, foreground.blue, foreground.alpha })));
        BackgroundSwatchPreview().Background(ColorBrush(ToHsva({
            background.red, background.green, background.blue, background.alpha })));
    }

    MainWindow::RgbaColor MainWindow::ToRgba(HsvaColor const& color) noexcept
    {
        auto hue = std::fmod(color.hue, 360.0);
        if (hue < 0.0)
        {
            hue += 360.0;
        }

        auto const saturation = std::clamp(color.saturation, 0.0, 1.0);
        auto const value = std::clamp(color.value, 0.0, 1.0);
        auto const chroma = value * saturation;
        auto const x = chroma * (1.0 - std::abs(std::fmod(hue / 60.0, 2.0) - 1.0));
        auto const match = value - chroma;

        double red{};
        double green{};
        double blue{};
        if (hue < 60.0)
        {
            red = chroma;
            green = x;
        }
        else if (hue < 120.0)
        {
            red = x;
            green = chroma;
        }
        else if (hue < 180.0)
        {
            green = chroma;
            blue = x;
        }
        else if (hue < 240.0)
        {
            green = x;
            blue = chroma;
        }
        else if (hue < 300.0)
        {
            red = x;
            blue = chroma;
        }
        else
        {
            red = chroma;
            blue = x;
        }

        return {
            ToByte(red + match),
            ToByte(green + match),
            ToByte(blue + match),
            ToByte(color.alpha) };
    }

    MainWindow::HsvaColor MainWindow::ToHsva(RgbaColor const& color) noexcept
    {
        auto const red = color.red / 255.0;
        auto const green = color.green / 255.0;
        auto const blue = color.blue / 255.0;
        auto const maximum = (std::max)({ red, green, blue });
        auto const minimum = (std::min)({ red, green, blue });
        auto const delta = maximum - minimum;

        double hue{};
        if (delta > std::numeric_limits<double>::epsilon())
        {
            if (maximum == red)
            {
                hue = 60.0 * std::fmod((green - blue) / delta, 6.0);
            }
            else if (maximum == green)
            {
                hue = 60.0 * (((blue - red) / delta) + 2.0);
            }
            else
            {
                hue = 60.0 * (((red - green) / delta) + 4.0);
            }
        }
        if (hue < 0.0)
        {
            hue += 360.0;
        }

        return {
            hue,
            maximum <= std::numeric_limits<double>::epsilon() ? 0.0 : delta / maximum,
            maximum,
            color.alpha / 255.0 };
    }

    Microsoft::UI::Xaml::Media::SolidColorBrush MainWindow::ColorBrush(HsvaColor const& color)
    {
        auto const rgba = ToRgba(color);
        return Microsoft::UI::Xaml::Media::SolidColorBrush(
            Windows::UI::Color{ rgba.alpha, rgba.red, rgba.green, rgba.blue });
    }
}
