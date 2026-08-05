#include "pch.h"
#include "MainWindow.xaml.h"

#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>
#include <limits>

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

    void MainWindow::RootGrid_Loaded(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& event_args)
    {
        ForegroundSwatchPreview().Background(ColorBrush(foreground_color_));
        BackgroundSwatchPreview().Background(ColorBrush(background_color_));
        ProjectToolOptionsToControls();
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
        std::swap(foreground_color_, background_color_);
        ForegroundSwatchPreview().Background(ColorBrush(foreground_color_));
        BackgroundSwatchPreview().Background(ColorBrush(background_color_));
    }

    void MainWindow::ResetColors_Click(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::RoutedEventArgs const& event_args)
    {
        CloseColorEditorBeforeExternalChange();
        foreground_color_ = { 0.0, 0.0, 0.0, 1.0 };
        background_color_ = { 0.0, 0.0, 1.0, 1.0 };
        ForegroundSwatchPreview().Background(ColorBrush(foreground_color_));
        BackgroundSwatchPreview().Background(ColorBrush(background_color_));
    }

    void MainWindow::ColorSlider_ValueChanged(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs const& event_args)
    {
        if (suppress_color_events_ || !color_edit_active_)
        {
            return;
        }

        auto& color = EditingTarget();
        color.hue = HueSlider().Value();
        color.saturation = SaturationSlider().Value() / 100.0;
        color.value = ValueSlider().Value() / 100.0;
        color.alpha = AlphaSlider().Value() / 100.0;
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

        auto const previous_alpha = EditingTarget().alpha;
        EditingTarget() = ToHsva({
            static_cast<std::uint8_t>(NumberBoxByte(RedNumberBox())),
            static_cast<std::uint8_t>(NumberBoxByte(GreenNumberBox())),
            static_cast<std::uint8_t>(NumberBoxByte(BlueNumberBox())),
            ToByte(previous_alpha) });
        EditingTarget().alpha = previous_alpha;
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

        auto const previous_alpha = EditingTarget().alpha;
        EditingTarget() = ToHsva({
            static_cast<std::uint8_t>((rgb >> 16) & 0xff),
            static_cast<std::uint8_t>((rgb >> 8) & 0xff),
            static_cast<std::uint8_t>(rgb & 0xff),
            ToByte(previous_alpha) });
        EditingTarget().alpha = previous_alpha;
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
            [[maybe_unused]] auto const closed = workspace_.CloseDocument(tab_document_ids_[index]);
            RefreshView();
        }
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
        PressureCurveComboBox().SelectedIndex(tool_options_.pressure_curve_preset);
        PressureGammaNumberBox().Value(tool_options_.pressure_gamma);
        StabilizerStrengthNumberBox().Value(tool_options_.stabilizer_strength);
        StabilizerSmoothingNumberBox().Value(tool_options_.stabilizer_smoothing);
        SensitivityDialog().XamlRoot(RootGrid().XamlRoot());
        [[maybe_unused]] auto const result = co_await SensitivityDialog().ShowAsync();
    }

    void MainWindow::SensitivityDialog_PrimaryButtonClick(
        [[maybe_unused]] Microsoft::UI::Xaml::Controls::ContentDialog const& sender,
        [[maybe_unused]] Microsoft::UI::Xaml::Controls::ContentDialogButtonClickEventArgs const& event_args)
    {
        tool_options_.pressure_curve_preset = (std::max)(0, PressureCurveComboBox().SelectedIndex());
        tool_options_.pressure_gamma = PressureGammaNumberBox().Value();
        tool_options_.stabilizer_strength = StabilizerStrengthNumberBox().Value();
        tool_options_.stabilizer_smoothing = StabilizerSmoothingNumberBox().Value();
    }

    void MainWindow::Canvas_PointerInput(
        [[maybe_unused]] Windows::Foundation::IInspectable const& sender,
        Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& event_args)
    {
        UpdatePointerDevice(event_args);
    }

    void MainWindow::ProjectToolOptionsToControls()
    {
        suppress_tool_option_events_ = true;
        BrushSizeNumberBox().Value(tool_options_.brush_size);
        HardnessNumberBox().Value(tool_options_.hardness);
        SpacingNumberBox().Value(tool_options_.spacing);
        FlowNumberBox().Value(tool_options_.flow);
        OpacityNumberBox().Value(tool_options_.opacity);
        PressureSizeCheckBox().IsChecked(tool_options_.pressure_affects_dab_size);
        PressureOpacityCheckBox().IsChecked(tool_options_.pressure_affects_opacity);
        StrokeStabilizerCheckBox().IsChecked(tool_options_.stroke_stabilizer);
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

        tool_options_.brush_size = value_or(BrushSizeNumberBox(), tool_options_.brush_size);
        tool_options_.hardness = value_or(HardnessNumberBox(), tool_options_.hardness);
        tool_options_.spacing = value_or(SpacingNumberBox(), tool_options_.spacing);
        tool_options_.flow = value_or(FlowNumberBox(), tool_options_.flow);
        tool_options_.opacity = value_or(OpacityNumberBox(), tool_options_.opacity);
        tool_options_.pressure_affects_dab_size = IsChecked(PressureSizeCheckBox());
        tool_options_.pressure_affects_opacity = IsChecked(PressureOpacityCheckBox());
        tool_options_.stroke_stabilizer = IsChecked(StrokeStabilizerCheckBox());
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

    void MainWindow::RefreshView()
    {
        auto const snapshot = workspace_.Snapshot();
        DocumentTitleText().Text(winrt::to_hstring(snapshot.document_title));
        StatusText().Text(winrt::to_hstring(std::format(
            "{} | Tool: {}",
            snapshot.status_message,
            winrt::to_string(selected_tool_))));
        RefreshDocumentTabs(snapshot);
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

        selected_tool_ = unbox_value<hstring>(selected_button.Tag());
    }

    void MainWindow::BeginColorEdit(
        bool const foreground,
        Microsoft::UI::Xaml::FrameworkElement const& anchor)
    {
        CloseColorEditorBeforeExternalChange();
        editing_foreground_ = foreground;
        original_edit_color_ = EditingTarget();
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

        auto const& color = EditingTarget();
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
        auto const brush = ColorBrush(EditingTarget());
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
        EditingTarget() = original_edit_color_;
        if (editing_foreground_)
        {
            ForegroundSwatchPreview().Background(ColorBrush(foreground_color_));
        }
        else
        {
            BackgroundSwatchPreview().Background(ColorBrush(background_color_));
        }
    }

    void MainWindow::FinishColorEdit(bool const apply)
    {
        if (!color_edit_active_)
        {
            return;
        }

        if (!apply)
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

    MainWindow::HsvaColor& MainWindow::EditingTarget() noexcept
    {
        return editing_foreground_ ? foreground_color_ : background_color_;
    }

    MainWindow::HsvaColor const& MainWindow::EditingTarget() const noexcept
    {
        return editing_foreground_ ? foreground_color_ : background_color_;
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

