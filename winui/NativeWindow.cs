using System.Runtime.InteropServices;
using Microsoft.UI;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Windows.Graphics;

namespace BaCafe.Settings;

internal abstract class NativeWindow
{
    internal readonly Window Window;
    private readonly SettingsApplication application;
    private readonly bool compact;
    private readonly nint hwnd;
    private bool placing;
    private bool closing;
    internal bool Shown { get; private set; }

    protected NativeWindow(SettingsApplication application, bool compact, FrameworkElement root)
    {
        this.application = application;
        this.compact = compact;
        Window = new Window { Title = compact ? "BA Cafe · 快捷菜单" : "BA Cafe · 设置", Content = root };
        hwnd = WinRT.Interop.WindowNative.GetWindowHandle(Window);
        if (compact)
        {
            var presenter = OverlappedPresenter.Create();
            presenter.SetBorderAndTitleBar(false, false);
            presenter.IsAlwaysOnTop = true;
            presenter.IsResizable = presenter.IsMinimizable = presenter.IsMaximizable = false;
            Window.AppWindow.SetPresenter(presenter);
            SetWindowLongPtr(hwnd, -20, (GetWindowLongPtr(hwnd, -20) | 0x80) & ~0x40000);
            Window.SystemBackdrop = new DesktopAcrylicBackdrop();
            Window.Activated += (_, args) =>
            {
                if (!placing && args.WindowActivationState == WindowActivationState.Deactivated) Hide();
            };
        }
        else
        {
            Window.SystemBackdrop = new MicaBackdrop();
        }
        Window.Closed += (_, args) =>
        {
            if (closing) return;
            args.Handled = true;
            Hide();
        };
        root.KeyboardAccelerators.Add(new KeyboardAccelerator
        {
            Key = Windows.System.VirtualKey.Escape
        });
        root.KeyboardAccelerators[^1].Invoked += (_, args) =>
        {
            if (!DialogOpen) { Hide(); args.Handled = true; }
        };
    }

    protected bool DialogOpen { get; set; }

    internal void Show(Point cursor)
    {
        placing = true;
        if (!Shown)
        {
            var info = new Monitor { Size = Marshal.SizeOf<Monitor>() };
            if (GetMonitorInfo(MonitorFromPoint(cursor, 2), ref info))
            {
                var area = info.Work;
                Window.AppWindow.Move(new PointInt32(area.Left, area.Top));
                double scale = Math.Max(96d, GetDpiForWindow(hwnd)) / 96d;
                int w = Math.Min((int)((compact ? 372 : 1000) * scale), area.Right - area.Left);
                int h = Math.Min((int)((compact ? 640 : 748) * scale), area.Bottom - area.Top);
                int x = compact ? Math.Clamp(cursor.X, area.Left, area.Right - w) : (area.Left + area.Right - w) / 2;
                int y = compact ? Math.Clamp(cursor.Y, area.Top, area.Bottom - h) : (area.Top + area.Bottom - h) / 2;
                Window.AppWindow.MoveAndResize(new RectInt32(x, y, w, h));
            }
            Window.AppWindow.Show();
        }
        else if (IsIconic(hwnd)) ShowWindow(hwnd, 9);
        Shown = true;
        Window.Activate();
        SetForegroundWindow(hwnd);
        placing = false;
        Announce();
    }

    internal void Hide()
    {
        if (!Shown) return;
        Shown = false;
        Window.AppWindow.Hide();
        Announce();
    }

    internal void Close()
    {
        closing = true;
        Window.Close();
    }

    private void Announce() => application.Send(new
    {
        type = "view", surface = compact ? "quick" : "full", hwnd = (ulong)hwnd
    });

    [StructLayout(LayoutKind.Sequential)]
    internal readonly record struct Point(int X, int Y);
    [StructLayout(LayoutKind.Sequential)]
    private struct Rect { internal int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)]
    private struct Monitor { internal int Size; internal Rect Bounds, Work; internal int Flags; }
    [DllImport("user32.dll")] private static extern nint MonitorFromPoint(Point point, uint flags);
    [DllImport("user32.dll", EntryPoint = "GetMonitorInfoW")] private static extern bool GetMonitorInfo(nint monitor, ref Monitor info);
    [DllImport("user32.dll")] private static extern uint GetDpiForWindow(nint hwnd);
    [DllImport("user32.dll")] private static extern bool SetForegroundWindow(nint hwnd);
    [DllImport("user32.dll")] private static extern bool IsIconic(nint hwnd);
    [DllImport("user32.dll")] private static extern bool ShowWindow(nint hwnd, int command);
    [DllImport("user32.dll", EntryPoint = "GetWindowLongPtrW")] private static extern nint GetWindowLongPtr(nint hwnd, int index);
    [DllImport("user32.dll", EntryPoint = "SetWindowLongPtrW")] private static extern nint SetWindowLongPtr(nint hwnd, int index, nint value);
}
