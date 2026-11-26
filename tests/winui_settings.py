"""Drive the WinUI controls and verify their commands reach the desktop pet."""
import configparser
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

from message_bubbles import enum_callback, user, wait_for, window


user.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
user.SetWindowPos.argtypes = [wintypes.HWND, wintypes.HWND, ctypes.c_int, ctypes.c_int,
                             ctypes.c_int, ctypes.c_int, wintypes.UINT]
user.WindowFromPoint.argtypes = [wintypes.POINT]
user.WindowFromPoint.restype = wintypes.HWND
user.GetAncestor.argtypes = [wintypes.HWND, wintypes.UINT]
user.GetAncestor.restype = wintypes.HWND
kernel = ctypes.WinDLL("kernel32", use_last_error=True)
kernel.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
kernel.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
kernel.CloseHandle.argtypes = [wintypes.HANDLE]


class ProcessEntry(ctypes.Structure):
    _fields_ = [
        ("size", wintypes.DWORD), ("usage", wintypes.DWORD), ("pid", wintypes.DWORD),
        ("heap", ctypes.c_size_t), ("module", wintypes.DWORD), ("threads", wintypes.DWORD),
        ("parent", wintypes.DWORD), ("priority", wintypes.LONG), ("flags", wintypes.DWORD),
        ("exe", wintypes.WCHAR * 260),
    ]


kernel.Process32FirstW.argtypes = kernel.Process32NextW.argtypes = [wintypes.HANDLE, ctypes.POINTER(ProcessEntry)]


def children(owner):
    snapshot = kernel.CreateToolhelp32Snapshot(2, 0)
    if snapshot == ctypes.c_void_p(-1).value:
        return []
    try:
        entry = ProcessEntry()
        entry.size = ctypes.sizeof(entry)
        found = kernel.Process32FirstW(snapshot, ctypes.byref(entry))
        pids = []
        while found:
            if entry.parent == owner and entry.exe == "BaCafe.Settings.exe":
                pids.append(entry.pid)
            found = kernel.Process32NextW(snapshot, ctypes.byref(entry))
        return pids
    finally:
        kernel.CloseHandle(snapshot)


AUTOMATION = r"""
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
$hwnd = [IntPtr][long]$env:BA_CAFE_TEST_HWND
for ($attempt = 0; $attempt -lt 25; $attempt++) {
    try { $root = [Windows.Automation.AutomationElement]::FromHandle($hwnd); break }
    catch [Windows.Automation.ElementNotAvailableException] { Start-Sleep -Milliseconds 100 }
}
if ($null -eq $root) { throw "Window has no UI Automation provider: $hwnd" }
$tree = [Windows.Automation.TreeScope]::Descendants
function Find-Control([string]$Name, $ControlType = $null) {
    $property = [Windows.Automation.AutomationElement]::NameProperty
    $condition = [Windows.Automation.PropertyCondition]::new($property, $Name)
    if ($null -ne $ControlType) {
        $kind = [Windows.Automation.PropertyCondition]::new(
            [Windows.Automation.AutomationElement]::ControlTypeProperty, $ControlType)
        $condition = [Windows.Automation.AndCondition]::new($condition, $kind)
    }
    for ($attempt = 0; $attempt -lt 32; $attempt++) {
        $element = $root.FindFirst($tree, $condition)
        if ($null -ne $element) { return $element }
        Start-Sleep -Milliseconds 80
    }
    throw "Control missing: $Name"
}
function Open-Page([string]$Name) {
    $item = Find-Control $Name ([Windows.Automation.ControlType]::ListItem)
    $item.GetCurrentPattern([Windows.Automation.SelectionItemPattern]::Pattern).Select()
}
function Click([string]$Name) {
    $item = Find-Control $Name
    $item.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke()
}
function Check-ImportHidden {
    $name = [Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::NameProperty, '导入模型')
    $button = $root.FindFirst($tree, $name)
    if ($null -ne $button -and !$button.Current.IsOffscreen) { throw 'Import is visible outside the Models page' }
}
function Model-Check([string]$Name, [string]$State) {
    $item = Find-Control $Name ([Windows.Automation.ControlType]::CheckBox)
    $toggle = $item.GetCurrentPattern([Windows.Automation.TogglePattern]::Pattern)
    if ($toggle.Current.ToggleState.ToString() -ne $State) { throw "Unexpected checkbox state for $Name" }
    return $item
}
function Action-Bounds([string[]]$Names) {
    foreach ($name in $Names) {
        $button = Find-Control $name ([Windows.Automation.ControlType]::Button)
        if ($button.Current.IsOffscreen) { throw "Button outside the visible menu: $name" }
        $button.Current.BoundingRectangle
    }
}
function Check-EqualSize($Bounds) {
    foreach ($item in $Bounds) {
        if ([Math]::Abs($item.Width - $Bounds[0].Width) -gt 1 -or
            [Math]::Abs($item.Height - $Bounds[0].Height) -gt 1) { throw 'Menu buttons have different sizes' }
    }
}
function Check-ModelActions([int]$Columns) {
    $library = @(Action-Bounds @('打开模型文件夹', '刷新模型库', '更改保存位置…', '恢复默认位置'))
    $models = @(Action-Bounds @('隐藏角色', '设为当前', '重命名…', '定位文件', '删除模型…', '统一角色大小', '隐藏当前角色'))
    Check-EqualSize ($library + $models)
    $gap = $library[1].Left - $library[0].Right
    foreach ($buttons in @($library, $models)) {
        for ($i = 0; $i -lt $buttons.Length; $i++) {
            $column = $i % $Columns
            if ([Math]::Abs($buttons[$i].Left - $library[$column].Left) -gt 1 -or
                [Math]::Abs($buttons[$i].Top - $buttons[$i - $column].Top) -gt 1 -or
                ($i -ge $Columns -and [Math]::Abs($buttons[$i].Top - $buttons[$i - $Columns].Bottom - $gap) -gt 1)) {
                throw 'Model actions do not align on a shared grid'
            }
        }
    }
}
"""


def automation(hwnd, script):
    command = ["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", AUTOMATION + "\n" + script]
    result = subprocess.run(command, env=dict(os.environ, BA_CAFE_TEST_HWND=str(hwnd)),
                            capture_output=True, timeout=15)
    assert result.returncode == 0, (result.stdout.decode(errors="replace"), result.stderr.decode(errors="replace"))
    return result.stdout.decode().strip()


def click_model_row(hwnd, label, clicks):
    point = automation(hwnd, f"""
        $row = Find-Control '{label}' ([Windows.Automation.ControlType]::ListItem)
        $rect = $row.Current.BoundingRectangle
        if ($row.Current.IsOffscreen -or $rect.IsEmpty) {{ throw 'Model row is outside the quick menu' }}
        '{{0}},{{1}}' -f [int]($rect.Left + 30), [int]($rect.Top + $rect.Height / 2)
    """)
    x, y = map(int, point.split(","))
    assert user.GetAncestor(user.WindowFromPoint(wintypes.POINT(x, y)), 2) == hwnd
    cursor = wintypes.POINT()
    assert user.GetCursorPos(ctypes.byref(cursor))
    try:
        assert user.SetCursorPos(x, y)
        for index in range(clicks):
            if index:
                time.sleep(.04)
            user.mouse_event(0x0002, 0, 0, 0, 0)
            user.mouse_event(0x0004, 0, 0, 0, 0)
    finally:
        user.SetCursorPos(cursor.x, cursor.y)


def top_window(title=None, owner=None, class_name=None, visible=True):
    selected = None

    @enum_callback
    def visit(hwnd, _):
        nonlocal selected
        if visible and not user.IsWindowVisible(hwnd):
            return True
        pid = wintypes.DWORD()
        user.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        if owner is not None and owner != pid.value:
            return True
        value = ctypes.create_unicode_buffer(128)
        if class_name is not None:
            user.GetClassNameW(hwnd, value, len(value))
            if value.value != class_name:
                return True
        if title is not None:
            user.GetWindowTextW(hwnd, value, len(value))
            if value.value != title:
                return True
        selected = hwnd, pid.value
        return False

    user.EnumWindows(visit, 0)
    return selected


def main(binary):
    binary = Path(binary).resolve(strict=True)
    assert (binary.parent / "winui" / "BaCafe.Settings.exe").is_file()
    artifacts = Path(__file__).resolve().parents[1] / "artifacts"
    artifacts.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ba-winui-", dir=artifacts) as scratch:
        data = Path(scratch) / "data"
        data.mkdir()
        agent_profile = Path(scratch) / "agent-profile"
        env = dict(os.environ, BA_NATIVE_PROGRAM_DIR=scratch, USERPROFILE=str(agent_profile),
                   CODEX_HOME=str(agent_profile / ".codex"), CLAUDE_CONFIG_DIR=str(agent_profile / ".claude"))
        pet = subprocess.Popen([binary, "--data-dir", data, "--settings"], env=env,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

        def view(title):
            def find():
                for pid in children(pet.pid):
                    if found := top_window(title=title, owner=pid):
                        return found
                return None
            return wait_for(find, limit=15)

        try:
            full, ui_pid = view("BA Cafe · 设置")
            assert ui_pid != pet.pid, "WinUI must run outside the renderer process"
            automation(full, r"""
                $nameProperty = [Windows.Automation.AutomationElement]::NameProperty
                $wandering = [Windows.Automation.PropertyCondition]::new($nameProperty, '允许散步')
                $cafe = [Windows.Automation.PropertyCondition]::new($nameProperty, '咖啡厅')
                $navigation = Find-Control '设置导航' ([Windows.Automation.ControlType]::List)
                if ($null -ne $navigation.FindFirst($tree, $cafe)) { throw 'Cafe is still in settings navigation' }
                if ($null -ne $root.FindFirst($tree, $wandering)) { throw 'Movement is still on the global page' }
                Check-ImportHidden
                Open-Page '移动'
                $null = Find-Control '允许散步'
                $null = Find-Control '时间与随机变化'
                $null = Find-Control '无输入等待'
                $null = Find-Control '视线追踪鼠标'
                $lookMode = Find-Control '视线追踪方式' ([Windows.Automation.ControlType]::ComboBox)
                $lookMode.GetCurrentPattern([Windows.Automation.ExpandCollapsePattern]::Pattern).Expand()
                $threshold = Find-Control '30° 后转向' ([Windows.Automation.ControlType]::ListItem)
                $threshold.GetCurrentPattern([Windows.Automation.SelectionItemPattern]::Pattern).Select()
                Check-ImportHidden
                Open-Page '战斗'
                $null = Find-Control '允许战斗走动'
                Check-ImportHidden
                Open-Page '模型管理'
                $null = Action-Bounds @('导入模型')
                Click '重命名…'
                $input = Find-Control '模型名称'
                $input.GetCurrentPattern([Windows.Automation.ValuePattern]::Pattern).SetValue('测试白子')
                Click '保存'
            """)

            def saved_name():
                path = data / "characters.json"
                return path.exists() and any(row["name"] == "测试白子"
                    for row in json.loads(path.read_text(encoding="utf-8"))["characters"])

            wait_for(saved_name, 6)
            full_bounds = wintypes.RECT()
            assert user.GetWindowRect(full, ctypes.byref(full_bounds))
            full_width = full_bounds.right - full_bounds.left
            full_height = full_bounds.bottom - full_bounds.top
            full_scale = user.GetDpiForWindow(full) / 96
            if full_width >= 790 * full_scale:
                automation(full, "Check-ModelActions 4")
                assert user.SetWindowPos(full, None, 0, 0, round(650 * full_scale), full_height, 0x16)
                try:
                    automation(full, "Check-ModelActions 2")
                finally:
                    assert user.SetWindowPos(full, None, 0, 0, full_width, full_height, 0x16)
            automation(full, r"""
                Open-Page 'Agent 消息'
                $null = Action-Bounds @('连接 Codex', '连接 Claude Code', '连接 Cursor', '复制通用命令')
                Click '连接 Codex'
                Click '连接 Claude Code'
                Click '连接 Cursor'
                $slider = Find-Control '不透明度' ([Windows.Automation.ControlType]::Slider)
                Check-ImportHidden
                $slider.GetCurrentPattern([Windows.Automation.RangeValuePattern]::Pattern).SetValue(64)
                Click '发送测试消息'
            """)
            for name, filename, event in ((".codex", "hooks.json", "UserPromptSubmit"),
                                          (".claude", "settings.json", "UserPromptSubmit"),
                                          (".cursor", "hooks.json", "afterAgentResponse")):
                config = agent_profile / name / filename
                wait_for(config.is_file)
                assert json.loads(config.read_text(encoding="utf-8"))["hooks"][event]
            bubble = wait_for(lambda: window(pet.pid, "BaCafeMessageBubble"))
            text = ctypes.create_unicode_buffer(256)
            user.GetWindowTextW(bubble, text, len(text))
            assert text.value == "测试白子\nBA Cafe Pets · 测试消息已送达。"

            cursor_hooks = json.loads((agent_profile / ".cursor/hooks.json").read_text(encoding="utf-8"))["hooks"]

            def cursor_hook(event, **payload):
                command = cursor_hooks[event][0]["command"]
                result = subprocess.run(["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", command],
                                        input=json.dumps(dict(hook_event_name=event, generation_id="cursor-fixture",
                                                              **payload), ensure_ascii=False).encode("utf-8"),
                                        capture_output=True, check=True, timeout=6)
                assert json.loads(result.stdout) == {}

            def bubble_is(expected):
                handle = window(pet.pid, "BaCafeMessageBubble")
                if handle:
                    user.GetWindowTextW(handle, text, len(text))
                    return text.value == "测试白子\n" + expected
                return False

            cursor_hook("beforeSubmitPrompt")
            wait_for(lambda: bubble_is("Cursor · 任务开始了哦..."), 5)
            cursor_hook("afterAgentResponse", text="预设命令保留中文。")
            wait_for(lambda: bubble_is("Cursor · 预设命令保留中文。"), 5)

            def setting(section, key, *, default=-1):
                cfg = configparser.ConfigParser(strict=False)
                raw = (data / "settings.ini").read_bytes()
                cfg.read_string(raw.decode("utf-16" if raw.startswith(b"\xff\xfe") else "utf-8-sig"))
                getter = cfg.get if isinstance(default, str) else cfg.getint
                return getter(section, key, fallback=default)

            wait_for(lambda: setting("Three", "CafeLookMode", default="") == "threshold", 5)
            wait_for(lambda: setting("Messages", "Opacity") == 64, 5)
            automation(full, "Open-Page '模型管理'; Click '导入模型'")
            picker, _ = wait_for(lambda: top_window(
                owner=pet.pid, title="导入模型（可按住 Ctrl 或 Shift 多选）"))
            user.PostMessageW(picker, 0x10, 0, 0)
            wait_for(lambda: not user.IsWindowVisible(picker))
            pet_input = wait_for(lambda: window(pet.pid, "BaCafeNativeInput"))

            def open_quick_menu():
                user.PostMessageW(pet_input, 0x204, 2, 0)
                user.PostMessageW(pet_input, 0x205, 0, 0)
                menu, quick_pid = view("BA Cafe · 快捷菜单")
                assert quick_pid == ui_pid
                return menu

            quick = open_quick_menu()
            assert not (user.GetWindowLongPtrW(quick, -16) & 0x00400000), "Quick menu has a Win32 dialog frame"
            bounds = wintypes.RECT()
            assert user.GetWindowRect(quick, ctypes.byref(bounds))
            scale = user.GetDpiForWindow(quick) / 96
            assert bounds.right - bounds.left <= 300 * scale
            assert bounds.bottom - bounds.top <= 416 * scale
            wandering = setting("Behavior", "Wandering")
            automation(quick, r"""
                $elements = $root.FindAll($tree, [Windows.Automation.Condition]::TrueCondition)
                if ($elements | Where-Object { $_.Current.Name -like '更改已自动保存*' }) { throw 'Unexpected quick menu status' }
                $modes = @(Action-Bounds @('咖啡厅', '战斗', '列队'))
                Check-EqualSize $modes
                if ($modes[0].Top -ne $modes[1].Top -or $modes[1].Top -ne $modes[2].Top -or
                    [Math]::Abs(($modes[1].Left - $modes[0].Right) - ($modes[2].Left - $modes[1].Right)) -gt 1) {
                    throw 'Mode buttons are not evenly spaced in one row'
                }
                $actions = @(Action-Bounds @('打开设置', '导入模型', '模型文件夹', '退出桌宠'))
                Check-EqualSize $actions
                if ($actions[0].Left -ne $actions[2].Left -or $actions[1].Left -ne $actions[3].Left -or
                    $actions[0].Top -ne $actions[1].Top -or $actions[2].Top -ne $actions[3].Top -or
                    [Math]::Abs(($actions[1].Left - $actions[0].Right) - ($actions[2].Top - $actions[0].Bottom)) -gt 1) {
                    throw 'Bottom actions are not evenly spaced in a two-by-two grid'
                }
                $label = Find-Control '允许散步' ([Windows.Automation.ControlType]::Text)
                $switch = Find-Control '允许散步' ([Windows.Automation.ControlType]::Button)
                $textBox = $label.Current.BoundingRectangle
                $switchBox = $switch.Current.BoundingRectangle
                if ($switchBox.Left -le $textBox.Left -or $textBox.Top -ge $switchBox.Bottom -or
                    $textBox.Bottom -le $switchBox.Top) { throw 'Quick menu switch is not on the right of its label' }
                $switch.GetCurrentPattern([Windows.Automation.TogglePattern]::Pattern).Toggle()
                Click '打开设置'
            """)
            wait_for(lambda: setting("Behavior", "Wandering") == (0 if wandering == 1 else 1), 5)
            full, reopened_pid = view("BA Cafe · 设置")
            assert reopened_pid == ui_pid

            tray, _ = wait_for(lambda: top_window(owner=pet.pid, class_name="BaCafeNativeTray", visible=False))

            def open_tray_menu():
                user.PostMessageW(tray, 0x803D, 0, 0x1007B)
                user.PostMessageW(tray, 0x803D, 1, 0x205)
                menu, tray_pid = view("BA Cafe · 托盘菜单")
                assert tray_pid == ui_pid and menu != quick and not user.IsWindowVisible(quick)
                return menu

            menu = open_tray_menu()
            automation(menu, r"""
                $null = Find-Control '测试白子' ([Windows.Automation.ControlType]::Text)
                $buttons = [Windows.Automation.PropertyCondition]::new(
                    [Windows.Automation.AutomationElement]::ControlTypeProperty, [Windows.Automation.ControlType]::Button)
                $actions = $root.FindAll($tree, $buttons) | Where-Object { -not $_.Current.IsOffscreen } | ForEach-Object { $_.Current.Name }
                if (($actions -join ',') -ne '打开设置,导入模型,模型文件夹,退出桌宠') { throw "Unexpected tray menu actions: $actions" }
                Check-EqualSize @(Action-Bounds $actions)
                Click '打开设置'
            """)
            extra = Path(scratch) / "model" / "测试备用模型.glb"
            shutil.copyfile(binary.parent / "model" / "Shiroko.glb", extra)
            automation(full, r"""
                Open-Page '模型管理'
                Click '刷新模型库'
                $active = Model-Check '测试白子' 'On'
                $hidden = Model-Check '测试备用模型' 'Off'
                $activeBox = $active.Current.BoundingRectangle
                $hiddenBox = $hidden.Current.BoundingRectangle
                if ($activeBox.IsEmpty -or $hiddenBox.IsEmpty -or
                    $activeBox.Left -ne $hiddenBox.Left -or $activeBox.Bottom -gt $hiddenBox.Top) {
                    throw 'The model checkboxes are not aligned in separate rows'
                }
                $hidden.GetCurrentPattern([Windows.Automation.TogglePattern]::Pattern).Toggle()
            """)

            def extra_active():
                rows = json.loads((data / "characters.json").read_text(encoding="utf-8"))["characters"]
                return next(row["active"] for row in rows if row["name"] == extra.stem)

            wait_for(extra_active, 6)
            automation(full, r"""
                $hidden = Model-Check '测试备用模型' 'On'
                $hidden.GetCurrentPattern([Windows.Automation.TogglePattern]::Pattern).Toggle()
            """)
            wait_for(lambda: not extra_active(), 6)
            quick = open_quick_menu()
            click_model_row(quick, "测试备用模型 · 已隐藏", 1)
            time.sleep(user.GetDoubleClickTime() / 1000)
            assert not extra_active(), "A single click in the quick roster toggled visibility"
            click_model_row(quick, "测试备用模型 · 已隐藏", 2)
            wait_for(extra_active, 6)
            click_model_row(quick, "测试备用模型 · 已显示", 2)
            wait_for(lambda: not extra_active(), 6)
            automation(quick, "Click '打开设置'")
            automation(full, r"""
                $null = Model-Check '测试备用模型' 'Off'
                $active = Model-Check '测试白子' 'On'
                $active.GetCurrentPattern([Windows.Automation.TogglePattern]::Pattern).Toggle()
            """)
            wait_for(lambda: not user.IsWindowVisible(pet_input))
            automation(full, "Open-Page '全局设置'; Check-ImportHidden")
            menu = open_tray_menu()
            automation(menu, "$null = Find-Control '没有活动模型'; Click '导入模型'")
            picker, _ = wait_for(lambda: top_window(
                owner=pet.pid, title="导入模型（可按住 Ctrl 或 Shift 多选）"))
            user.PostMessageW(picker, 0x10, 0, 0)
            wait_for(lambda: not user.IsWindowVisible(picker))
            automation(full, "$null = Find-Control '本地模型库'; $null = Action-Bounds @('导入模型')")
            automation(open_tray_menu(), "Click '退出桌宠'")
            assert pet.wait(timeout=6) == 0
            print("WinUI 3 model and tray controls, three Agent presets, and test message passed.")
        finally:
            if pet.poll() is None:
                pet.terminate()
                pet.wait(timeout=5)


if __name__ == "__main__":
    main(sys.argv[1])
