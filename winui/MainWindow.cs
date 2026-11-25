using System.Text.Json;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Automation;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;

namespace BaCafe.Settings;

internal sealed class MainWindow : NativeWindow
{
    private static readonly string[] Pages = ["全局设置", "咖啡厅", "战斗", "模型管理", "Codex 消息"];
    private static readonly string[] Icons = ["\uE713", "\uE80F", "\uE7FC", "\uE716", "\uE8BD"];
    private readonly SettingsApplication app;
    private readonly IReadOnlyList<Field> fields;
    private readonly ListView navigation = new() { SelectionMode = ListViewSelectionMode.Single };
    private readonly Dictionary<int, StackPanel> views = [];
    private readonly Dictionary<int, List<OptionEditor>> editors = [];
    private readonly TextBlock title = Controls.Text(Pages[0], 28, true);
    private readonly TextBlock subtitle = Controls.Text("");
    private readonly TextBlock status = Controls.Text("", 12);
    private readonly TextBlock sampling = Controls.Text("", 12);
    private readonly TextBlock modelPath = Controls.Text("", 12);
    private readonly TextBlock hookStatus = Controls.Text("连接后，在 Codex 的 Hooks 设置中确认信任。", 12);
    private readonly ScrollViewer scroll = new() { VerticalScrollBarVisibility = ScrollBarVisibility.Auto };
    private readonly ProgressBar progress = new() { Visibility = Visibility.Collapsed };
    private readonly ToggleSwitch startup = new() { OnContent = "", OffContent = "" };
    private readonly Slider opacity = new() { Minimum = 0, Maximum = 100, StepFrequency = 1, Width = 260 };
    private readonly Slider blur = new() { Minimum = 0, Maximum = 24, StepFrequency = 1, Width = 260 };
    private readonly TextBlock opacityLabel = Controls.Text("");
    private readonly TextBlock blurLabel = Controls.Text("");
    private readonly ModelRoster roster;
    private readonly Button import, changeFolder, resetFolder, uniformSize, hideActive;
    private JsonElement state;
    private int page = -1;
    private bool applying;

    internal MainWindow(SettingsApplication app, IReadOnlyList<Field> fields) : this(app, fields, new Grid()) { }

    private MainWindow(SettingsApplication app, IReadOnlyList<Field> fields, Grid root) : base(app, false, root)
    {
        this.app = app;
        this.fields = fields;
        AutomationProperties.SetAutomationId(root, "BaCafe.WinUI.Settings");
        AutomationProperties.SetName(navigation, "设置导航");
        root.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(197) });
        root.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        var sidebar = new Grid { Padding = new Thickness(12, 16, 8, 16), RowSpacing = 10 };
        sidebar.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        sidebar.RowDefinitions.Add(new RowDefinition { Height = new GridLength(1, GridUnitType.Star) });
        sidebar.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        var brand = Controls.Text("BA Cafe", 23, true);
        brand.Margin = new Thickness(10, 0, 0, 8);
        sidebar.Children.Add(brand);
        for (int i = 0; i < Pages.Length; ++i)
        {
            var content = Controls.Row(new FontIcon { Glyph = Icons[i], FontSize = 16 }, Controls.Text(Pages[i]));
            var item = new ListViewItem { Content = content, MinHeight = 40 };
            AutomationProperties.SetName(item, Pages[i]);
            navigation.Items.Add(item);
        }
        navigation.SelectionChanged += (_, _) =>
        {
            if (navigation.SelectedIndex >= 0) Navigate(navigation.SelectedIndex);
        };
        Grid.SetRow(navigation, 1);
        sidebar.Children.Add(navigation);
        var commands = new StackPanel { Spacing = 6, Margin = new Thickness(4, 0, 0, 0) };
        commands.Children.Add(Controls.Button("使用说明", async () => await Help()));
        commands.Children.Add(Controls.Button("退出桌宠", () => app.Send(new { type = "exit" })));
        Grid.SetRow(commands, 2);
        sidebar.Children.Add(commands);
        root.Children.Add(sidebar);
        var frame = new Grid { Padding = new Thickness(24, 12, 24, 16), RowSpacing = 12 };
        frame.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        frame.RowDefinitions.Add(new RowDefinition { Height = new GridLength(1, GridUnitType.Star) });
        frame.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        var header = new Grid { ColumnSpacing = 12 };
        header.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        header.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
        var labels = new StackPanel { Spacing = 5 };
        labels.Children.Add(title);
        labels.Children.Add(subtitle);
        header.Children.Add(labels);
        import = Controls.Button("导入模型", () => app.Send(new { type = "import" }), true);
        import.VerticalAlignment = VerticalAlignment.Center;
        Grid.SetColumn(import, 1);
        header.Children.Add(import);
        frame.Children.Add(header);
        Grid.SetRow(scroll, 1);
        frame.Children.Add(scroll);
        var footer = new StackPanel { Spacing = 5 };
        footer.Children.Add(status);
        footer.Children.Add(progress);
        Grid.SetRow(footer, 2);
        frame.Children.Add(footer);
        Grid.SetColumn(frame, 1);
        root.Children.Add(frame);
        roster = new ModelRoster(app, false, open => DialogOpen = open);
        changeFolder = Controls.Button("更改保存位置…", () => app.Send(new { type = "change_models_folder" }));
        resetFolder = Controls.Button("恢复默认位置", () => app.Send(new { type = "reset_models_folder" }));
        uniformSize = Controls.Button("统一角色大小", () => app.Send(new { type = "uniform_size" }));
        hideActive = Controls.Button("隐藏当前角色", () => app.Send(new { type = "close_character" }));
        startup.Toggled += (_, _) => { if (!applying) app.Send(new { type = "set_autostart", enabled = startup.IsOn }); };
        opacity.ValueChanged += (_, _) => BubbleChanged();
        blur.ValueChanged += (_, _) => BubbleChanged();
        Navigate(0);
    }

    internal void Navigate(int index)
    {
        if (page == index) return;
        page = index;
        if (navigation.SelectedIndex != index) navigation.SelectedIndex = index;
        title.Text = Pages[index];
        if (!views.TryGetValue(index, out var content))
        {
            content = new StackPanel { Spacing = 10, MaxWidth = 820, HorizontalAlignment = HorizontalAlignment.Stretch };
            if (index == 3) BuildModels(content);
            else if (index == 4) BuildCodex(content);
            else BuildOptions(content, index);
            views[index] = content;
        }
        scroll.Content = content;
        scroll.ChangeView(null, 0, null, true);
        RefreshPage();
        if (index == 3) app.Send(new { type = "refresh_models" });
    }

    private void BuildOptions(StackPanel content, int index)
    {
        var controls = new List<OptionEditor>();
        foreach (var field in fields.Where(field => field.Page == index))
        {
            string? section = field.Key switch
            {
                "mode" => "角色与显示", "wandering" => "移动与跟随", "idleSeconds" => "时间与随机变化",
                "cafeTracking" => "视线追踪", "battleWalking" => "战斗动作", _ => null
            };
            if (section != null) content.Children.Add(Controls.Text(section, 17, true));
            var control = new OptionEditor(field, value => app.Send(new { type = "option", key = field.Key, value }));
            controls.Add(control);
            content.Children.Add(control.View);
            if (field.Key == "renderQuality") content.Children.Add(sampling);
        }
        editors[index] = controls;
        if (index == 0)
        {
            content.Children.Add(Controls.Button("恢复默认设置", () => app.Send(new { type = "reset_preferences" })));
            content.Children.Add(Controls.Text("应用", 17, true));
            content.Children.Add(Controls.Card("开机自启", "登录 Windows 时自动启动桌宠", startup, "\uE7E8"));
        }
    }

    private void BuildModels(StackPanel content)
    {
        content.Children.Add(Controls.Text("本地模型库", 17, true));
        content.Children.Add(modelPath);
        content.Children.Add(Controls.Row(
            Controls.Button("打开模型文件夹", () => app.Send(new { type = "open_models_folder" })),
            Controls.Button("刷新模型库", () => app.Send(new { type = "refresh_models" }))));
        content.Children.Add(Controls.Row(changeFolder, resetFolder));
        content.Children.Add(Controls.Text("已导入的模型", 17, true));
        content.Children.Add(roster.View);
        content.Children.Add(Controls.Row(uniformSize, hideActive));
    }

    private void BuildCodex(StackPanel content)
    {
        content.Children.Add(Controls.Button("连接 Codex", () => app.Send(new { type = "connect_codex" }), true));
        content.Children.Add(hookStatus);
        content.Children.Add(Controls.Card("不透明度", "", Controls.Row(opacity, opacityLabel)));
        content.Children.Add(Controls.Card("背景模糊", "", Controls.Row(blur, blurLabel)));
        AutomationProperties.SetName(opacity, "不透明度");
        AutomationProperties.SetName(blur, "背景模糊");
        content.Children.Add(Controls.Button("预览气泡", () => app.Send(new { type = "preview_bubble" })));
    }

    internal void CodexResult(string detail, bool error)
    {
        hookStatus.Text = error ? "无法连接 Codex。 " + detail : detail;
    }

    internal void Apply(JsonElement state, string message, string samples)
    {
        this.state = state;
        var mode = Controls.Mode(state.Property("value").String("mode"));
        subtitle.Text = $"{state.String("name", "桌面角色")}  ·  {mode}模式  ·  {state.Integer("activeCount")} 个角色已显示";
        status.Text = message;
        sampling.Text = samples;
        import.IsEnabled = !state.Boolean("busy");
        progress.Visibility = state.Boolean("busy") ? Visibility.Visible : Visibility.Collapsed;
        progress.Value = state.Integer("percent");
        RefreshPage();
    }

    private void RefreshPage()
    {
        if (page == -1 || state.ValueKind != JsonValueKind.Object) return;
        applying = true;
        if (editors.TryGetValue(page, out var controls))
            foreach (var control in controls) control.Apply(state);
        if (page == 0)
        {
            var value = state.Property("startup");
            startup.IsEnabled = value.Boolean("available");
            startup.IsOn = value.Boolean("enabled");
        }
        else if (page == 3)
        {
            modelPath.Text = state.String("modelsDirectory");
            changeFolder.IsEnabled = !state.Boolean("busy");
            resetFolder.IsEnabled = changeFolder.IsEnabled && !state.Boolean("modelsDirectoryDefault");
            uniformSize.IsEnabled = hideActive.IsEnabled = state.Integer("activeCount") > 0;
            roster.Apply(state);
        }
        else if (page == 4)
        {
            var value = state.Property("value");
            opacity.Value = value.Integer("bubbleOpacity", 88);
            blur.Value = value.Integer("bubbleBlur", 8);
            BubbleLabels();
        }
        applying = false;
    }

    private void BubbleLabels()
    {
        opacityLabel.Text = $"{opacity.Value:0}%";
        blurLabel.Text = $"{blur.Value:0} px";
    }

    private void BubbleChanged()
    {
        if (applying) return;
        BubbleLabels();
        app.Send(new { type = "bubble_settings", bubbleOpacity = (int)opacity.Value, bubbleBlur = (int)blur.Value });
    }

    private async Task Help()
    {
        DialogOpen = true;
        try
        {
            await new ContentDialog
            {
                XamlRoot = navigation.XamlRoot, Title = "使用说明", CloseButtonText = "关闭",
                Content = Controls.Text("右键打开快捷菜单。开启全局旋转后，可用右键或中键拖动旋转。\n\n" +
                    "左键拖动移动角色，滚轮调整大小。选中角色后按空格可切换显示，按 Esc 关闭菜单。\n\n" +
                    "咖啡厅支持点击反应和视线追踪。战斗模式支持按键攻击，句号、回车、分号、问号请求换弹。\n\n" +
                    "双击播放 Exs，三击中断或切换咖啡厅、战斗和列队，跳过模型缺少的动作。")
            }.ShowAsync();
        }
        finally { DialogOpen = false; }
    }
}
