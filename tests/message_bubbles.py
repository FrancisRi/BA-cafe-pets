"""Exercise the desktop bubble with both Codex event formats."""
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time


user = ctypes.WinDLL("user32", use_last_error=True)
enum_callback = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
user.EnumWindows.argtypes = [enum_callback, wintypes.LPARAM]
user.IsWindowVisible.argtypes = [wintypes.HWND]
user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
user.GetClassNameW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
user.GetWindowRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
user.GetWindowLongPtrW.argtypes = [wintypes.HWND, ctypes.c_int]
user.GetWindowLongPtrW.restype = ctypes.c_ssize_t
user.GetDpiForWindow.argtypes = [wintypes.HWND]
user.SetProcessDpiAwarenessContext.argtypes = [ctypes.c_void_p]
user.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))


def window(pid, class_name):
    found = None

    @enum_callback
    def visit(hwnd, _):
        nonlocal found
        owner = wintypes.DWORD()
        user.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid and user.IsWindowVisible(hwnd):
            name = ctypes.create_unicode_buffer(128)
            user.GetClassNameW(hwnd, name, len(name))
            if name.value == class_name:
                found = hwnd
                return False
        return True

    user.EnumWindows(visit, 0)
    return found


def wait_for(condition, limit=12):
    deadline = time.monotonic() + limit
    while time.monotonic() < deadline:
        result = condition()
        if result:
            return result
        time.sleep(.05)
    raise AssertionError("Timed out waiting for the message bubble")


def run(binary, sender):
    binary = Path(binary).resolve(strict=True)
    sender = Path(sender).resolve(strict=True)
    artifacts = Path(__file__).resolve().parents[1] / "artifacts"
    artifacts.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ba-message-test-", dir=artifacts) as scratch:
        data = Path(scratch) / "data"
        data.mkdir()
        settings = data / "settings.ini"
        settings.write_text(
            "[Behavior]\nWandering=0\n[Messages]\nOpacity=80\nBlur=8\n",
            encoding="ascii",
        )
        env = dict(os.environ, BA_NATIVE_PROGRAM_DIR=scratch)
        pet = subprocess.Popen([binary, "--data-dir", data, "--diagnostics"], env=env)
        try:
            # A valid send confirms that the scene's endpoint is ready.
            first_event = json.dumps(
                {"type": "agent-turn-complete", "last-assistant-message": "第一条 Codex 回复"},
                ensure_ascii=False,
            )

            def send_completion():
                assert pet.poll() is None, "Desktop pet exited during startup"
                return subprocess.run([sender, "--codex-notify", first_event], timeout=3).returncode == 0

            wait_for(send_completion)
            hwnd = wait_for(lambda: window(pet.pid, "BaCafeMessageBubble"))
            style = user.GetWindowLongPtrW(hwnd, -20)
            assert style & 0x08080020 == 0x08080020, "Bubble must be layered, click-through and non-activating"
            first = wintypes.RECT()
            assert user.GetWindowRect(hwnd, ctypes.byref(first))
            actor = wait_for(lambda: window(pet.pid, "BaCafeNativePet"))
            expected_width = round(328 * user.GetDpiForWindow(actor) / 96)
            assert first.right - first.left == expected_width, (first.right - first.left, expected_width)
            event = json.dumps(
                {
                    "hook_event_name": "Stop",
                    "last_assistant_message": "第二条消息\n多行文本验证\n新一行",
                    "transcript_path": "x" * 40_000,
                },
                ensure_ascii=False,
            )
            reply = subprocess.run(
                [sender, "--codex-hook"], input=event.encode("utf-8"),
                capture_output=True, timeout=3, check=True,
            )
            assert json.loads(reply.stdout) == {}, "Codex Stop hook returns an empty decision"
            assert subprocess.run([sender, "--codex-notify", "invalid"], timeout=3).returncode != 0

            def next_message():
                rect = wintypes.RECT()
                ready = window(pet.pid, "BaCafeMessageBubble")
                return ready and user.GetWindowRect(ready, ctypes.byref(rect)) and (
                    rect.bottom - rect.top > first.bottom - first.top
                )

            wait_for(next_message)
            wait_for(lambda: not window(pet.pid, "BaCafeMessageBubble"))
        except Exception:
            for log in (data / "logs").glob("pet.*"):
                print(log.read_text(errors="replace").splitlines()[-10:], file=sys.stderr)
            raise
        finally:
            if pet.poll() is None:
                pet.terminate()
                pet.wait(timeout=5)
    print("Codex notify and Stop hook reached the DPI-scaled click-through bubble.")


if __name__ == "__main__":
    run(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else sys.argv[1])
