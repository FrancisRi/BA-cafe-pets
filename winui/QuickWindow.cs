using System.Text.Json;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Automation;
using Microsoft.UI.Xaml.Controls;

namespace BaCafe.Settings;

internal sealed class QuickWindow : NativeWindow
{
    private const int SingleModelHeight = 416;
    private const int MultipleModelHeight = 456;
    private readonly TextBlock title = Controls.Text("桌面角色", 17, true);
    private readonly TextBlock count = Controls.Text("", 12);
    private readonly TextBlock status = Controls.Text("", 12);
    private readonly ModelRoster roster;
    private readonly Dictionary<string, Button> modes = [];
    private readonly Dictionary<string, (TextBlock Label, ToggleSwitch Switch)> toggles = [];
    private readonly Button import;
    private bool applying;

    internal QuickWindow(SettingsApplication app) : this(app, new ScrollViewer { VerticalScrollBarVisibility = ScrollBarVisibility.Auto }) { }

    private QuickWindow(SettingsApplication app, ScrollViewer root)
        : base(app, root, "BA Cafe · 快捷菜单", 296, SingleModelHeight, compact: true)
    {
        AutomationProperties.SetAutomationId(root, "BaCafe.WinUI.Quick");
        var content = new StackPanel { Padding = new Thickness(12), Spacing = 5 };
        root.Content = content;
        var heading = new Grid { ColumnSpacing = 8 };
        heading.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        heading.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
        title.MaxLines = 1;
        title.TextTrimming = TextTrimming.CharacterEllipsis;
        count.VerticalAlignment = VerticalAlignment.Center;
        Grid.SetColumn(count, 1);
        heading.Children.Add(title);
        heading.Children.Add(count);
        content.Children.Add(heading);
        roster = new ModelRoster(app, true, open => DialogOpen = open);
        content.Children.Add(roster.View);
        foreach (var mode in new[] { "cafe", "battle", "formation" })
        {
            var button = MenuButton(Controls.Mode(mode), () => app.Send(new { type = "option", key = "mode", value = mode }));
            modes.Add(mode, button);
        }
        content.Children.Add(Controls.ButtonGrid(3, modes.Values.ToArray()));
        var switches = new StackPanel { Spacing = 1 };
        content.Children.Add(switches);
        foreach (var (key, label) in new[] { ("inspectionMode", "观赏模式"), ("follow", "位置跟随鼠标"),
            ("wandering", "允许散步"), ("tracking", "视线追踪鼠标"), ("battleWalking", "允许战斗走动"), ("rightDragRotation", "右键拖动旋转") })
        {
            var text = Controls.Text(label);
            text.VerticalAlignment = VerticalAlignment.Center;
            var toggle = new ToggleSwitch { OnContent = "", OffContent = "", MinWidth = 0, Height = 32 };
            AutomationProperties.SetLabeledBy(toggle, text);
            toggles.Add(key, (text, toggle));
            toggle.Toggled += (_, _) =>
            {
                if (!applying) app.Send(new { type = "option", key = key == "tracking" ? tracking : key, value = toggle.IsOn });
            };
            var row = new Grid { ColumnSpacing = 12 };
            row.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
            row.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
            row.Children.Add(text);
            row.Children.Add(toggle);
            Grid.SetColumn(toggle, 1);
            switches.Children.Add(row);
        }
        import = MenuButton("导入模型", () => app.OpenFull(import: true));
        content.Children.Add(Controls.ButtonGrid(2, MenuButton("打开设置", () => app.OpenFull(), true), import,
            MenuButton("模型文件夹", app.OpenModelsFolder), MenuButton("退出桌宠", () => app.Send(new { type = "exit" }))));
        content.Children.Add(status);
    }

    private static Button MenuButton(string label, Action action, bool primary = false)
    {
        var button = Controls.Button(label, action, primary);
        button.Padding = new Thickness(8, 4, 8, 4);
        button.MinHeight = 30;
        return button;
    }

    private string tracking = "cafeTracking";

    internal void Apply(JsonElement state, string message)
    {
        applying = true;
        title.Text = state.String("name", "桌面角色");
        count.Text = $"已显示 {state.Integer("activeCount")} / {state.Integer("maximum", 5)}";
        status.Text = message;
        status.Visibility = message.Length == 0 ? Visibility.Collapsed : Visibility.Visible;
        var models = state.Property("characters");
        Height = models.ValueKind == JsonValueKind.Array && models.GetArrayLength() > 1 ? MultipleModelHeight : SingleModelHeight;
        roster.Apply(state);
        import.IsEnabled = !state.Boolean("busy");
        var values = state.Property("value");
        string current = values.String("mode");
        tracking = current == "battle" ? "look" : "cafeTracking";
        toggles["tracking"].Label.Text = tracking == "look" ? "战斗朝向鼠标" : "视线追踪鼠标";
        foreach (var (mode, button) in modes)
        {
            button.IsEnabled = mode != current && !state.Boolean("busy") &&
                (mode == "cafe" || state.Boolean(mode == "battle" ? "battleAvailable" : "formationAvailable"));
        }
        foreach (var (key, (label, toggle)) in toggles)
        {
            var option = key == "tracking" ? tracking : key;
            toggle.IsEnabled = Controls.Enabled(option, option is "look" or "battleWalking" ? 2 : 0, state);
            label.Opacity = toggle.IsEnabled ? 1 : 0.55;
            toggle.IsOn = values.Boolean(option) && !(current == "formation" && !toggle.IsEnabled);
        }
        applying = false;
    }
}
