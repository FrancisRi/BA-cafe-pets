using System.Text.Json;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Automation;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;

namespace BaCafe.Settings;

internal static class Controls
{
    internal static TextBlock Text(string text, double size = 14, bool title = false)
    {
        var block = new TextBlock { Text = text, FontSize = size, TextWrapping = TextWrapping.Wrap };
        if (title) block.FontWeight = Microsoft.UI.Text.FontWeights.SemiBold;
        return block;
    }

    internal static Button Button(string title, Action onClick, bool primary = false)
    {
        var button = new Button { Content = title };
        if (primary && Application.Current.Resources.TryGetValue("AccentButtonStyle", out var style))
            button.Style = (Style)style;
        button.Click += (_, _) => onClick();
        return button;
    }

    internal static StackPanel Row(params UIElement[] elements)
    {
        var row = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8 };
        foreach (var element in elements) row.Children.Add(element);
        return row;
    }

    internal static Grid ButtonGrid(int columns, params Button[] buttons)
    {
        var grid = new Grid { ColumnSpacing = 6, RowSpacing = 6 };
        foreach (var button in buttons)
        {
            button.HorizontalAlignment = HorizontalAlignment.Stretch;
            button.VerticalAlignment = VerticalAlignment.Stretch;
            button.HorizontalContentAlignment = HorizontalAlignment.Center;
            grid.Children.Add(button);
        }
        ArrangeButtonGrid(grid, buttons, columns);
        return grid;
    }

    internal static Grid AdaptiveButtonGrid(params Button[] buttons)
    {
        foreach (var button in buttons) button.MinHeight = 36;
        var grid = ButtonGrid(4, buttons);
        grid.SizeChanged += (_, args) => ArrangeButtonGrid(grid, buttons, args.NewSize.Width switch
        {
            >= 520 => 4,
            >= 260 => 2,
            _ => 1
        });
        return grid;
    }

    private static void ArrangeButtonGrid(Grid grid, Button[] buttons, int columns)
    {
        if (grid.ColumnDefinitions.Count == columns) return;
        grid.ColumnDefinitions.Clear();
        grid.RowDefinitions.Clear();
        for (var column = 0; column < columns; column++)
            grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        for (var index = 0; index < buttons.Length; index++)
        {
            if (index % columns == 0)
                grid.RowDefinitions.Add(new RowDefinition { Height = new GridLength(1, GridUnitType.Star) });
            Grid.SetColumn(buttons[index], index % columns);
            Grid.SetRow(buttons[index], index / columns);
        }
    }

    internal static FrameworkElement Card(string title, string detail, FrameworkElement control, string icon = "", TextBlock? status = null)
    {
        var content = new Grid { ColumnSpacing = 12 };
        content.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        content.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
        var labels = new StackPanel { Spacing = 4, VerticalAlignment = VerticalAlignment.Center };
        var heading = Row();
        if (icon.Length != 0) heading.Children.Add(new FontIcon { Glyph = icon, FontSize = 16 });
        heading.Children.Add(Text(title, 14, true));
        labels.Children.Add(heading);
        if (detail.Length != 0)
        {
            var hint = Text(detail, 12); hint.Opacity = 0.72;
            labels.Children.Add(hint);
        }
        control.VerticalAlignment = VerticalAlignment.Center;
        AutomationProperties.SetName(control, title);
        content.Children.Add(labels);
        content.Children.Add(control);
        Grid.SetColumn(control, 1);
        if (status != null)
        {
            content.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
            content.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
            content.RowSpacing = 6;
            Grid.SetRow(status, 1);
            Grid.SetColumnSpan(status, 2);
            content.Children.Add(status);
        }
        var card = new Border
        {
            Child = content, Padding = new Thickness(14, 9, 14, 9), MinHeight = 56,
            CornerRadius = new CornerRadius(8), BorderThickness = new Thickness(1)
        };
        void Theme()
        {
            byte tint = card.ActualTheme == ElementTheme.Dark ? (byte)255 : (byte)0;
            card.Background = new SolidColorBrush(global::Windows.UI.Color.FromArgb(10, tint, tint, tint));
            card.BorderBrush = new SolidColorBrush(global::Windows.UI.Color.FromArgb(30, tint, tint, tint));
        }
        card.Loaded += (_, _) => Theme();
        card.ActualThemeChanged += (_, _) => Theme();
        return card;
    }

    internal static string Mode(string mode) => mode switch { "battle" => "战斗", "formation" => "列队", _ => "咖啡厅" };

    internal static bool Enabled(string key, int page, JsonElement state)
    {
        if (state.Boolean("busy") || (page == 2 && !state.Boolean("battleAvailable"))) return false;
        var value = state.Property("value");
        if (value.String("mode") == "formation" &&
            key is not ("mode" or "renderQuality" or "outlineMode" or "outlineWidth" or "rightDragRotation" or "inspectionMode"))
            return false;
        if (value.Boolean("inspectionMode") &&
            key is "follow" or "wandering" or "cafeTracking" or "look" or "battleWalking" or "rightDragRotation"
                or "followNear" or "followFar" or "cafeLookMode")
            return false;
        return key switch
        {
            "follow" => state.Boolean("canFollow"),
            "followNear" or "followFar" => state.Boolean("canFollow") && value.Boolean("follow"),
            "cafeLookMode" => value.Boolean("cafeTracking", true),
            "outlineWidth" => value.Integer("outlineMode", 2) != 0,
            _ => true
        };
    }
}

internal sealed class OptionEditor
{
    private readonly Field field;
    private readonly Action<object> change;
    private readonly Control input;
    private readonly NumberBox? exact;
    private IReadOnlyList<Choice> choices = [];
    private bool refreshing;
    internal FrameworkElement View { get; }

    private sealed record Choice(string Label, object Value);

    internal OptionEditor(Field field, Action<object> change, TextBlock? status = null)
    {
        this.field = field;
        this.change = change;
        FrameworkElement contents;
        switch (field.Kind)
        {
            case 0:
                var toggle = new ToggleSwitch { OnContent = "", OffContent = "", MinWidth = 42 };
                toggle.Toggled += (_, _) => Changed(toggle.IsOn);
                contents = input = toggle;
                break;
            case 1:
                var number = new NumberBox
                {
                    Minimum = field.Min, Maximum = field.Max, Width = 126,
                    SmallChange = field.Min % 1 == 0 ? 1 : 0.5,
                    SpinButtonPlacementMode = NumberBoxSpinButtonPlacementMode.Compact
                };
                number.ValueChanged += (_, args) => { if (double.IsFinite(args.NewValue)) Changed(args.NewValue); };
                contents = input = number;
                break;
            case 2:
                var combo = new ComboBox { MinWidth = 160, DisplayMemberPath = nameof(Choice.Label) };
                combo.SelectionChanged += (_, _) => { if (combo.SelectedItem is Choice item) Changed(item.Value); };
                contents = input = combo;
                break;
            default:
                var slider = new Slider { Minimum = field.Min, Maximum = field.Max, StepFrequency = 1, Width = 132 };
                exact = new NumberBox { Width = 70, Minimum = field.Min, Maximum = field.Max, SmallChange = 1 };
                slider.ValueChanged += (_, args) => { if (!refreshing) exact.Value = args.NewValue; };
                exact.ValueChanged += (_, args) => { if (double.IsFinite(args.NewValue)) Changed(Math.Round(args.NewValue)); };
                contents = Controls.Row(slider, exact); input = slider;
                break;
        }
        View = Controls.Card(field.Title, field.Detail, contents, field.Icon, status);
    }

    private void Changed(object value)
    {
        if (!refreshing) change(value);
    }

    internal void Apply(JsonElement state)
    {
        refreshing = true;
        var value = state.Property("value");
        input.IsEnabled = Controls.Enabled(field.Key, field.Page, state);
        switch (input)
        {
            case ToggleSwitch toggle:
                toggle.IsOn = value.Boolean(field.Key) && !(value.String("mode") == "formation" && !input.IsEnabled);
                break;
            case ComboBox combo:
                UpdateChoices(combo, state, value);
                break;
            case NumberBox number:
                double n = value.Number(field.Key, field.Min);
                if (number.Value != n) number.Value = n;
                break;
            case Slider slider:
                double width = value.Number(field.Key, 100);
                if (slider.Value != width) slider.Value = width;
                if (exact!.Value != width) exact.Value = width;
                exact.IsEnabled = input.IsEnabled;
                break;
        }
        refreshing = false;
    }

    private void UpdateChoices(ComboBox combo, JsonElement state, JsonElement value)
    {
        IReadOnlyList<Choice> next = field.Key switch
        {
            "mode" => Modes(state),
            "renderQuality" => [new("省电 · 无 MSAA", 0), new("均衡 · 2× MSAA", 1), new("清晰 · 4× MSAA", 2)],
            "outlineMode" => [new("无", 0), new("轮廓", 1), new("逐网格线条", 2)],
            _ => [new("平滑追踪", "smooth"), new("30° 后转向", "threshold")]
        };
        if (!choices.SequenceEqual(next))
        {
            choices = next;
            combo.ItemsSource = choices;
        }
        object current = field.Key is "mode" or "cafeLookMode" ? value.String(field.Key) : value.Integer(field.Key);
        var selected = choices.FirstOrDefault(item => item.Value.Equals(current));
        if (!Equals(combo.SelectedItem, selected)) combo.SelectedItem = selected;
    }

    private static IReadOnlyList<Choice> Modes(JsonElement state)
    {
        var modes = new List<Choice> { new("咖啡厅", "cafe") };
        if (state.Boolean("battleAvailable")) modes.Add(new("战斗", "battle"));
        if (state.Boolean("formationAvailable")) modes.Add(new("列队", "formation"));
        return modes;
    }
}
