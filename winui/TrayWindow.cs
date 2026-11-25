using System.Text.Json;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Automation;
using Microsoft.UI.Xaml.Controls;

namespace BaCafe.Settings;

internal sealed class TrayWindow : NativeWindow
{
    private readonly TextBlock title = Controls.Text("没有活动模型", 15, true);
    private readonly Button import;

    internal TrayWindow(SettingsApplication app) : this(app, new ScrollViewer { VerticalScrollBarVisibility = ScrollBarVisibility.Auto }) { }

    private TrayWindow(SettingsApplication app, ScrollViewer root)
        : base(app, root, "BA Cafe · 托盘菜单", 208, 200, compact: true)
    {
        AutomationProperties.SetAutomationId(root, "BaCafe.WinUI.Tray");
        var content = new StackPanel { Padding = new Thickness(10), Spacing = 3 };
        root.Content = content;
        title.Margin = new Thickness(8, 2, 8, 5);
        title.MaxLines = 1;
        title.TextTrimming = TextTrimming.CharacterEllipsis;
        content.Children.Add(title);
        content.Children.Add(ActionButton("打开设置", () => app.OpenFull()));
        import = ActionButton("导入模型", () => app.OpenFull(import: true));
        content.Children.Add(import);
        content.Children.Add(ActionButton("模型文件夹", app.OpenModelsFolder));
        content.Children.Add(ActionButton("退出桌宠", () => app.Send(new { type = "exit" })));
    }

    private static Button ActionButton(string label, Action action)
    {
        var button = Controls.Button(label, action);
        if (Application.Current.Resources.TryGetValue("SubtleButtonStyle", out var style)) button.Style = (Style)style;
        button.HorizontalAlignment = HorizontalAlignment.Stretch;
        button.HorizontalContentAlignment = HorizontalAlignment.Left;
        button.Padding = new Thickness(8, 5, 8, 5);
        button.MinHeight = 34;
        return button;
    }

    internal void Apply(JsonElement state)
    {
        title.Text = state.String("activeModelName", "没有活动模型");
        import.IsEnabled = !state.Boolean("busy");
    }
}
