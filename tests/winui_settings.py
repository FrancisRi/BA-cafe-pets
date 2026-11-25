"""Drive the WinUI controls and verify their commands reach the desktop pet."""
import configparser
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from message_bubbles import enum_callback, user, wait_for, window


user.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
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
    $item = Find-Control $Name
    $item.GetCurrentPattern([Windows.Automation.SelectionItemPattern]::Pattern).Select()
}
function Click([string]$Name) {
    $item = Find-Control $Name
    $item.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke()
}
"""


def automation(hwnd, script):
    command = ["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", AUTOMATION + "\n" + script]
    result = subprocess.run(command, env=dict(os.environ, BA_CAFE_TEST_HWND=str(hwnd)),
                            capture_output=True, timeout=15)
    assert result.returncode == 0, (result.stdout.decode(errors="replace"), result.stderr.decode(errors="replace"))


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
        env = dict(os.environ, BA_NATIVE_PROGRAM_DIR=scratch)
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
                Open-Page '咖啡厅'
                $null = Find-Control '视线追踪鼠标'
                Open-Page '战斗'
                $null = Find-Control '允许战斗走动'
                Open-Page '模型管理'
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
            automation(full, r"""
                Open-Page 'Codex 消息'
                $slider = Find-Control '不透明度' ([Windows.Automation.ControlType]::Slider)
                $slider.GetCurrentPattern([Windows.Automation.RangeValuePattern]::Pattern).SetValue(64)
                Click '预览气泡'
            """)
            bubble = wait_for(lambda: window(pet.pid, "BaCafeMessageBubble"))
            text = ctypes.create_unicode_buffer(256)
            user.GetWindowTextW(bubble, text, len(text))
            assert text.value == "测试白子\n任务开始了哦..."

            def opacity_saved():
                cfg = configparser.ConfigParser(strict=False)
                raw = (data / "settings.ini").read_bytes()
                cfg.read_string(raw.decode("utf-16" if raw.startswith(b"\xff\xfe") else "utf-8-sig"))
                return cfg.getint("Messages", "Opacity", fallback=-1) == 64

            wait_for(opacity_saved, 5)
            automation(full, "Click '导入模型'")
            picker, _ = wait_for(lambda: top_window(
                owner=pet.pid, title="导入模型（可按住 Ctrl 或 Shift 多选）"))
            user.PostMessageW(picker, 0x10, 0, 0)
            wait_for(lambda: not user.IsWindowVisible(picker))
            tray, _ = wait_for(lambda: top_window(owner=pet.pid, class_name="BaCafeNativeTray", visible=False))
            user.PostMessageW(tray, 0x803D, 0, 0x1007B)
            user.PostMessageW(tray, 0x803D, 1, 0x205)
            quick, quick_pid = view("BA Cafe · 快捷菜单")
            assert quick_pid == ui_pid
            automation(quick, "Click '打开设置'")
            full, reopened_pid = view("BA Cafe · 设置")
            assert reopened_pid == ui_pid
            automation(full, "Click '退出桌宠'")
            assert pet.wait(timeout=6) == 0
            print("WinUI 3 settings, model rename, Codex preview and sliders, and tray quick menu passed.")
        finally:
            if pet.poll() is None:
                pet.terminate()
                pet.wait(timeout=5)


if __name__ == "__main__":
    main(sys.argv[1])
