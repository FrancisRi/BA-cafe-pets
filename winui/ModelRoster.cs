using System.Text.Json;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Automation;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;

namespace BaCafe.Settings;

internal sealed class ModelRoster
{
    private readonly SettingsApplication app;
    private readonly bool compact;
    private readonly Action<bool> dialogVisibility;
    private readonly ListView list = new() { SelectionMode = ListViewSelectionMode.Single };
    private readonly TextBlock empty = Controls.Text("尚未导入模型");
    private readonly Button toggle, select, rename, locate, remove;
    private readonly Button[] activeModelActions = [];
    private ModelRow[] rows = [];
    private int activeCount, maximum;
    private string current = "";
    private Model? SelectedModel => (list.SelectedItem as ModelRow)?.Model;

    internal StackPanel View { get; } = new() { Spacing = 8 };
    private sealed record Model(string Id, string Name, bool Active, bool Available, bool Ready, bool Loading, bool Failed, bool Managed)
    {
        internal string Status => Loading ? "加载中" : Failed ? "加载失败" :
            !Available ? "文件不存在" : Active ? "已显示" : "已隐藏";
        internal static Model From(JsonElement row) => new(row.String("id"), row.String("name"), row.Boolean("active"),
            row.Boolean("available"), row.Boolean("ready"), row.Boolean("loading"), row.Boolean("failed"), row.Boolean("managed"));
    }

    private sealed class ModelRow : ListViewItem
    {
        private readonly CheckBox? check;
        private readonly TextBlock? name, status;
        private bool applying;

        internal Model Model { get; private set; }

        internal ModelRow(Model model, bool compact, Action<ModelRow, bool> setActive)
        {
            Model = model;
            if (compact)
            {
                DoubleTapped += (_, args) =>
                {
                    setActive(this, !Model.Active);
                    args.Handled = true;
                };
                return;
            }

            MinHeight = 40;
            HorizontalContentAlignment = HorizontalAlignment.Stretch;
            var columns = new Grid { ColumnSpacing = 8 };
            columns.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
            columns.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
            columns.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
            check = new CheckBox { MinWidth = 0, Width = 32, VerticalAlignment = VerticalAlignment.Center };
            name = Controls.Text("");
            name.MaxLines = 2;
            name.TextTrimming = TextTrimming.CharacterEllipsis;
            name.VerticalAlignment = VerticalAlignment.Center;
            status = Controls.Text("", 12);
            status.VerticalAlignment = VerticalAlignment.Center;
            Grid.SetColumn(name, 1);
            Grid.SetColumn(status, 2);
            columns.Children.Add(check);
            columns.Children.Add(name);
            columns.Children.Add(status);
            Content = columns;
            check.Checked += (_, _) => { if (!applying) setActive(this, true); };
            check.Unchecked += (_, _) => { if (!applying) setActive(this, false); };
        }

        internal void Apply(Model model, bool canToggle)
        {
            Model = model;
            if (check == null)
            {
                Content = $"{model.Name} · {model.Status}";
                return;
            }

            name!.Text = model.Name;
            status!.Text = model.Status;
            AutomationProperties.SetName(this, model.Name);
            AutomationProperties.SetName(check, model.Name);
            applying = true;
            check.IsChecked = model.Active;
            check.IsEnabled = canToggle;
            applying = false;
        }
    }

    internal ModelRoster(SettingsApplication app, bool compact, Action<bool> dialogVisibility)
    {
        this.app = app;
        this.compact = compact;
        this.dialogVisibility = dialogVisibility;
        list.MaxHeight = compact ? 76 : 320;
        AutomationProperties.SetName(list, "已导入的模型");
        if (compact) AutomationProperties.SetHelpText(list, "双击模型名称或按空格显示或隐藏角色");
        list.SelectionChanged += (_, _) => UpdateActions();
        list.AddHandler(UIElement.KeyDownEvent, new KeyEventHandler(KeyDown), true);
        toggle = Controls.Button("显示角色", () => { if (SelectedModel is Model model) Toggle(model); });
        select = Controls.Button("设为当前", () => Command("roster_select"));
        rename = Controls.Button("重命名…", async () => { if (SelectedModel is Model model) await Rename(model); });
        locate = Controls.Button("定位文件", () => Command("model_locate"));
        remove = Controls.Button("删除模型…", async () => { if (SelectedModel is Model model) await Remove(model); });
        View.Children.Add(empty);
        View.Children.Add(list);
        if (!compact)
        {
            activeModelActions =
            [
                Controls.Button("统一角色大小", () => app.Send(new { type = "uniform_size" })),
                Controls.Button("隐藏当前角色", () => app.Send(new { type = "close_character" }))
            ];
            View.Children.Add(Controls.AdaptiveButtonGrid([toggle, select, rename, locate, remove, .. activeModelActions]));
            list.RightTapped += RightTapped;
        }
    }

    internal void Apply(JsonElement state)
    {
        activeCount = state.Integer("activeCount");
        maximum = state.Integer("maximum", 5);
        current = state.String("selectedId");
        var next = state.Items("characters").Select(Model.From).ToArray();
        if (!next.Select(model => model.Id).SequenceEqual(rows.Select(row => row.Model.Id)))
        {
            var selected = SelectedModel?.Id ?? current;
            rows = next.Select(model => new ModelRow(model, compact, RowToggled)).ToArray();
            list.ItemsSource = rows;
            list.SelectedItem = rows.FirstOrDefault(row => row.Model.Id == selected);
        }
        if (list.SelectedItem == null && current.Length > 0)
            list.SelectedItem = rows.FirstOrDefault(row => row.Model.Id == current);
        for (var index = 0; index < rows.Length; index++)
            rows[index].Apply(next[index], CanToggle(next[index]));
        empty.Visibility = rows.Length == 0 ? Visibility.Visible : Visibility.Collapsed;
        list.Visibility = rows.Length == 0 ? Visibility.Collapsed : Visibility.Visible;
        foreach (var action in activeModelActions) action.IsEnabled = activeCount > 0;
        UpdateActions();
    }

    private bool CanToggle(Model model) => model.Active || (model.Available && activeCount < maximum);
    private bool CanSelect(Model model) => model.Active && model.Ready && model.Id != current;
    private void UpdateActions()
    {
        var model = SelectedModel;
        toggle.Content = model?.Active == true ? "隐藏角色" : "显示角色";
        toggle.IsEnabled = model != null && CanToggle(model);
        select.IsEnabled = model != null && CanSelect(model);
        rename.IsEnabled = model != null;
        locate.IsEnabled = model?.Available == true;
        remove.IsEnabled = model != null && !model.Loading;
        remove.Content = model?.Managed == true ? "删除模型…" : "移除记录…";
    }

    private void Command(string type)
    {
        if (SelectedModel is Model model) app.Send(new { type, id = model.Id });
    }

    private void RowToggled(ModelRow row, bool active)
    {
        list.SelectedItem = row;
        SetActive(row.Model, active);
    }

    private void Toggle(Model model) => SetActive(model, !model.Active);

    private void SetActive(Model model, bool active)
    {
        if (CanToggle(model)) app.Send(new { type = "roster_toggle", id = model.Id, active });
    }

    private async void KeyDown(object sender, KeyRoutedEventArgs args)
    {
        if (SelectedModel is not Model model) return;
        if (args.Key == Windows.System.VirtualKey.Space && args.OriginalSource is not CheckBox)
        {
            Toggle(model);
            args.Handled = true;
        }
        else if (!compact && args.Key == Windows.System.VirtualKey.F2) { args.Handled = true; await Rename(model); }
        else if (!compact && args.Key == Windows.System.VirtualKey.Delete && !model.Loading)
        {
            args.Handled = true;
            await Remove(model);
        }
    }

    private void RightTapped(object sender, RightTappedRoutedEventArgs args)
    {
        for (var source = args.OriginalSource as DependencyObject; source != null && source != list;
            source = VisualTreeHelper.GetParent(source))
        {
            if (source is not ModelRow row) continue;
            var model = row.Model;
            list.SelectedItem = row;
            var flyout = new MenuFlyout();
            Add(model.Active ? "隐藏角色" : "显示角色", CanToggle(model), () => Toggle(model));
            Add("设为当前角色", CanSelect(model), () => app.Send(new { type = "roster_select", id = model.Id }));
            Add("在文件夹中定位", model.Available, () => app.Send(new { type = "model_locate", id = model.Id }));
            Add("重命名…", true, async () => await Rename(model));
            Add(model.Managed ? "删除模型…" : "移除模型记录…", !model.Loading, async () => await Remove(model));
            flyout.ShowAt(list, new FlyoutShowOptions { Position = args.GetPosition(list) });
            args.Handled = true;
            return;
            void Add(string title, bool enabled, Action action)
            {
                var item = new MenuFlyoutItem { Text = title, IsEnabled = enabled };
                item.Click += (_, _) => action();
                flyout.Items.Add(item);
            }
        }
    }

    private async Task Rename(Model model)
    {
        var input = new TextBox { Header = "模型名称", Text = model.Name, MaxLength = 120 };
        AutomationProperties.SetName(input, "模型名称");
        var hint = Controls.Text("");
        var content = new StackPanel { Spacing = 8 };
        content.Children.Add(input);
        content.Children.Add(hint);
        var dialog = new ContentDialog
        {
            XamlRoot = list.XamlRoot, Title = "重命名模型", Content = content,
            PrimaryButtonText = "保存", CloseButtonText = "取消", DefaultButton = ContentDialogButton.Primary
        };
        dialog.Opened += (_, _) => { input.Focus(FocusState.Programmatic); input.SelectAll(); };
        dialog.PrimaryButtonClick += (_, args) =>
        {
            var name = input.Text.Trim();
            if (name.Length is > 0 and <= 120 && name.All(c => !char.IsControl(c) && c is not ('\u2028' or '\u2029'))) return;
            args.Cancel = true;
            hint.Text = "请输入 1–120 个字符的单行名称。";
        };
        if (await Show(dialog) == ContentDialogResult.Primary)
            app.Send(new { type = "model_rename", id = model.Id, name = input.Text.Trim() });
    }

    private async Task Remove(Model model)
    {
        var dialog = new ContentDialog
        {
            XamlRoot = list.XamlRoot, Title = model.Managed ? "删除模型" : "移除模型记录",
            Content = model.Managed ? $"将「{model.Name}」的本地模型副本移入回收站？" : $"从列表中移除「{model.Name}」？",
            PrimaryButtonText = model.Managed ? "删除" : "移除", CloseButtonText = "取消",
            DefaultButton = ContentDialogButton.Close
        };
        if (await Show(dialog) == ContentDialogResult.Primary)
            app.Send(new { type = "model_delete", id = model.Id });
    }

    private async Task<ContentDialogResult> Show(ContentDialog dialog)
    {
        dialogVisibility(true);
        try { return await dialog.ShowAsync(); }
        finally { dialogVisibility(false); }
    }
}
