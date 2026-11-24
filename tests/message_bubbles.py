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
user.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
user.FindWindowExW.argtypes = [wintypes.HWND, wintypes.HWND, wintypes.LPCWSTR, wintypes.LPCWSTR]
user.FindWindowExW.restype = wintypes.HWND
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


def endpoint_exists(pid):
    target = None
    while True:
        target = user.FindWindowExW(-3, target, "BaCafeNativeMessages.v1", None)
        if not target:
            return False
        owner = wintypes.DWORD()
        user.GetWindowThreadProcessId(target, ctypes.byref(owner))
        if owner.value == pid:
            return True


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
        codex_dir = Path(scratch) / "codex"
        session_id = "01a1c82b-1234-7456-87ab-3388f356b467"
        transcript = codex_dir / "sessions" / "2026" / "11" / "11" / f"rollout-2026-11-11T09-13-59-{session_id}.jsonl"
        transcript.parent.mkdir(parents=True)
        env = dict(os.environ, BA_NATIVE_PROGRAM_DIR=scratch, CODEX_HOME=str(codex_dir))
        pet = subprocess.Popen([binary, "--data-dir", data, "--diagnostics"], env=env)
        try:
            wait_for(lambda: endpoint_exists(pet.pid))
            turn = "test-turn"

            def hook(kind, **fields):
                event = {"hook_event_name": kind, "turn_id": turn, "session_id": session_id,
                         "transcript_path": str(transcript), **fields}
                reply = subprocess.run(
                    [sender, "--codex-hook"], input=json.dumps(event, ensure_ascii=False).encode("utf-8"),
                    capture_output=True, timeout=3, check=True,
                )
                assert json.loads(reply.stdout) == {}, "The hook returns an empty decision"

            def message_is(expected):
                hwnd = window(pet.pid, "BaCafeMessageBubble")
                if not hwnd:
                    return False
                value = ctypes.create_unicode_buffer(2048)
                user.GetWindowTextW(hwnd, value, len(value))
                return hwnd if value.value.split("\n", 1)[-1] == expected else False

            def append_record(*texts, owner=None, phase="commentary"):
                entries = [
                    {"type": "response_item", "payload": {
                        "type": "message", "role": "assistant", "phase": phase,
                        "internal_chat_message_metadata_passthrough": {"turn_id": owner or turn},
                        "content": [{"type": "output_text", "text": text}],
                    }}
                    for text in texts
                ]
                with transcript.open("a", encoding="utf-8") as target:
                    target.write("".join(json.dumps(entry, ensure_ascii=False) + "\n" for entry in entries))

            hook("UserPromptSubmit", prompt="内部提示词", transcript_path=None)
            wait_for(lambda: message_is("任务开始了哦..."))
            hwnd = wait_for(lambda: window(pet.pid, "BaCafeMessageBubble"))
            style = user.GetWindowLongPtrW(hwnd, -20)
            assert style & 0x08080020 == 0x08080020, "Bubble must be layered, click-through and non-activating"
            first = wintypes.RECT()
            assert user.GetWindowRect(hwnd, ctypes.byref(first))
            actor = wait_for(lambda: window(pet.pid, "BaCafeNativePet"))
            expected_width = round(352 * user.GetDpiForWindow(actor) / 96)
            assert first.right - first.left == expected_width, (first.right - first.left, expected_width)
            append_record("历史任务的进度", owner="other-turn")
            append_record("正在检查现有代码。", "正在运行测试。")
            wait_for(lambda: message_is("正在检查现有代码。"), limit=5)
            append_record("其他任务的输出", owner="other-turn")
            append_record("由 Stop 提供的回复", phase="final_answer")
            wait_for(lambda: message_is("正在运行测试。"), limit=4)
            hook("Stop", last_assistant_message="已完成验证。\n测试均通过。", fixture_padding="x" * 40_000)
            wait_for(lambda: message_is("已完成验证。\n测试均通过。"), limit=4)
            append_record("任务结束后的输出")
            assert message_is("已完成验证。\n测试均通过。")
            wait_for(lambda: not window(pet.pid, "BaCafeMessageBubble"))
            turn = "completion-only-turn"
            append_record("补发第一条进度。", "补发第二条进度。")
            hook("Stop", last_assistant_message="补发后的最终回复。")
            wait_for(lambda: message_is("补发第一条进度。"), limit=4)
            wait_for(lambda: message_is("补发第二条进度。"), limit=4)
            wait_for(lambda: message_is("补发后的最终回复。"), limit=4)
            assert subprocess.run([sender, "--codex-notify", "invalid"], timeout=3).returncode != 0
            turn = "interrupted-turn"
            hook("UserPromptSubmit", prompt="准备中断")
            wait_for(lambda: message_is("任务开始了哦..."), limit=4)
            hook("Interrupt")
            wait_for(lambda: message_is("任务已中断。"), limit=4)
            wait_for(lambda: not window(pet.pid, "BaCafeMessageBubble"))
            completion = json.dumps({"type": "agent-turn-complete", "last-assistant-message": "旧版 CLI 回复"})
            subprocess.run([sender, "--codex-notify", completion], timeout=3, check=True)
            wait_for(lambda: message_is("旧版 CLI 回复"))
        except Exception:
            for log in (data / "logs").glob("pet.*"):
                print(log.read_text(errors="replace").splitlines()[-10:], file=sys.stderr)
            raise
        finally:
            if pet.poll() is None:
                pet.terminate()
                pet.wait(timeout=5)
    print("Codex start, live commentary, completion, interrupt and CLI notify reached the bubble.")


if __name__ == "__main__":
    run(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else sys.argv[1])
