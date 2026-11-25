using System.Text.Json;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Automation;
using Microsoft.UI.Xaml.Controls;

namespace BaCafe.Settings;

internal sealed class QuickWindow : NativeWindow
{
    private readonly TextBlock title = Controls.Text("桌面角色", 20, true);
    private readonly TextBlock count = Controls.Text("", 12);
    private readonly TextBlock status = Controls.Text("", 12);
    private readonly ModelRoster roster;
    private readonly Dictionary<string, Button> modes = [];
    private readonly Dictionary<string, ToggleSwitch> toggles = [];
    private readonly Button import;
    private bool applying;

    internal QuickWindow(SettingsApplication app) : this(app, new ScrollViewer()) { }

    private QuickWindow(SettingsApplication app, ScrollViewer root) : base(app, true, root)
    {
        AutomationProperties.SetAutomationId(root, "BaCafe.WinUI.Quick");
        var content = new StackPanel { Padding = new Thickness(16), Spacing = 7 };
        root.Content = content;
        content.Children.Add(title);
        content.Children.Add(count);
        roster = new ModelRoster(app, true, open => DialogOpen = open);
        content.Children.Add(roster.View);
        var modeRow = Controls.Row();
        foreach (var mode in new[] { "cafe", "battle", "formation" })
        {
            var button = Controls.Button(Controls.Mode(mode), () => app.Send(new { type = "option", key = "mode", value = mode }));
            modes.Add(mode, button);
            modeRow.Children.Add(button);
        }
        content.Children.Add(modeRow);
        foreach (var (key, label) in new[] { ("inspectionMode", "观赏模式"), ("follow", "位置跟随鼠标"),
            ("wandering", "允许散步"), ("tracking", "视线追踪鼠标"), ("battleWalking", "允许战斗走动"), ("rightDragRotation", "右键拖动旋转") })
        {
            var toggle = new ToggleSwitch { Header = label, OnContent = "", OffContent = "" };
            toggles.Add(key, toggle);
            toggle.Toggled += (_, _) =>
            {
                if (!applying) app.Send(new { type = "option", key = key == "tracking" ? tracking : key, value = toggle.IsOn });
            };
            content.Children.Add(toggle);
        }
        content.Children.Add(Controls.Row(Controls.Button("打开设置", () => app.OpenFull(), true),
            Controls.Button("模型文件夹", () => { app.Send(new { type = "open_models_folder" }); Hide(); })));
        import = Controls.Button("导入模型", () => app.OpenFull(import: true));
        content.Children.Add(Controls.Row(import, Controls.Button("退出桌宠", () => app.Send(new { type = "exit" }))));
        content.Children.Add(status);
    }

    private string tracking = "cafeTracking";

    internal void Apply(JsonElement state, string message)
    {
        applying = true;
        title.Text = state.String("name", "桌面角色");
        count.Text = $"已导入的模型 · {state.Integer("activeCount")} / 5 已显示";
        status.Text = message;
        roster.Apply(state);
        import.IsEnabled = !state.Boolean("busy");
        var values = state.Property("value");
        string current = values.String("mode");
        tracking = current == "battle" ? "look" : "cafeTracking";
        toggles["tracking"].Header = tracking == "look" ? "战斗朝向鼠标" : "视线追踪鼠标";
        foreach (var (mode, button) in modes)
        {
            button.IsEnabled = mode != current && !state.Boolean("busy") &&
                (mode == "cafe" || state.Boolean(mode == "battle" ? "battleAvailable" : "formationAvailable"));
        }
        foreach (var (key, toggle) in toggles)
        {
            var option = key == "tracking" ? tracking : key;
            toggle.IsEnabled = Controls.Enabled(option, option is "look" or "battleWalking" ? 2 : 0, state);
            toggle.IsOn = values.Boolean(option) && !(current == "formation" && !toggle.IsEnabled);
        }
        applying = false;
    }
}
