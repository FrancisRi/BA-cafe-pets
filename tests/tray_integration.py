from pathlib import Path
import ctypes as c
from ctypes import wintypes as w
import json,subprocess,time,uuid,shutil,zipfile,os,sys
ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'artifacts/tray'/('integration-'+uuid.uuid4().hex)
PROGRAM=WORK/'application'
with zipfile.ZipFile(Path(sys.argv[1]).resolve()) as bundle: bundle.extractall(PROGRAM)
SCRATCH=WORK/'profile'
SCRATCH.mkdir()
user = c.WinDLL('user32', use_last_error=True)
kernel = c.WinDLL('kernel32', use_last_error=True)
kernel.CreateFileW.argtypes = [w.LPCWSTR,w.DWORD,w.DWORD,w.LPVOID,w.DWORD,w.DWORD,w.HANDLE]
kernel.CreateFileW.restype = w.HANDLE
kernel.GetFileSizeEx.argtypes = [w.HANDLE,c.POINTER(c.c_longlong)]
kernel.ReadFile.argtypes = [w.HANDLE,w.LPVOID,w.DWORD,c.POINTER(w.DWORD),w.LPVOID]
kernel.CloseHandle.argtypes = [w.HANDLE]
CALLBACK = c.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
user.EnumWindows.argtypes = [CALLBACK, w.LPARAM]
user.GetWindowThreadProcessId.argtypes = [w.HWND, c.POINTER(w.DWORD)]
user.GetClassNameW.argtypes = [w.HWND, w.LPWSTR, c.c_int]
user.GetDlgItem.argtypes = [w.HWND, c.c_int]
user.GetDlgItem.restype = w.HWND
user.IsWindowVisible.argtypes = [w.HWND]
user.IsWindowEnabled.argtypes = [w.HWND]
user.SendMessageTimeoutW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM, w.UINT, w.UINT, c.POINTER(c.c_size_t)]
user.PostMessageW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM]
user.ShowWindow.argtypes = [w.HWND, c.c_int]
user.EnumChildWindows.argtypes = [w.HWND, CALLBACK, w.LPARAM]
user.GetDlgCtrlID.argtypes = [w.HWND]

environment=os.environ.copy()
environment.pop('BA_NATIVE_PROGRAM_DIR',None)
environment.pop('BA_NATIVE_DATA_DIR',None)
process = subprocess.Popen([str(PROGRAM/'BA-Cafe-Pets.exe'),'--data-dir',str(SCRATCH),'--settings','--diagnostics'],cwd=PROGRAM,env=environment)

def windows(cls):
    found = []
    @CALLBACK
    def visit(hwnd, unused):
        pid = w.DWORD()
        user.GetWindowThreadProcessId(hwnd, c.byref(pid))
        if pid.value == process.pid:
            name = c.create_unicode_buffer(128)
            user.GetClassNameW(hwnd, name, 128)
            if name.value == cls and user.IsWindowVisible(hwnd):
                found.append(hwnd)
        return True
    user.EnumWindows(visit, 0)
    return found

def wait_for(check, label, seconds=12):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError(f'Host exited ({process.returncode}): {label}')
        value = check()
        if value:
            return value
        time.sleep(0.1)
    raise RuntimeError(f'Timed out: {label}')

def send(hwnd, message, wp=0, lp=0):
    result = c.c_size_t()
    if not user.SendMessageTimeoutW(hwnd, message, wp, lp, 2, 3000, c.byref(result)):
        raise RuntimeError(f'Native message timed out: {message:#x}')
    return result.value

def read_roster():
    # Share DELETE as well as READ/WRITE: ordinary Python file reads on Windows
    # can prevent the host's atomic replacement during this polling window.
    deadline = time.monotonic() + 2
    while True:
        try:
            handle=kernel.CreateFileW(str(SCRATCH/'characters.json'),0x80000000,7,None,3,0x80,None)
            if handle==w.HANDLE(-1).value:raise c.WinError(c.get_last_error())
            try:
                size=c.c_longlong();read=w.DWORD()
                if not kernel.GetFileSizeEx(handle,c.byref(size)):raise c.WinError(c.get_last_error())
                buffer=c.create_string_buffer(size.value)
                if not kernel.ReadFile(handle,buffer,size.value,c.byref(read),None):raise c.WinError(c.get_last_error())
                return json.loads(buffer.raw[:read.value].decode('utf-8'))
            finally:kernel.CloseHandle(handle)
        except (PermissionError, FileNotFoundError, json.JSONDecodeError):
            if time.monotonic() >= deadline:
                raise
            time.sleep(0.01)

def class_name(hwnd):
    name = c.create_unicode_buffer(128)
    user.GetClassNameW(hwnd, name, 128)
    return name.value


def panel():
    result=[]
    @CALLBACK
    def visit(hwnd,_):
        if class_name(hwnd)=='BaCafeNativeSettings':result.append(hwnd)
        return True
    user.EnumChildWindows(full,visit,0)
    return result[0]

def sampling_text():
    label=user.GetDlgItem(panel(),2111)
    assert label,'Global settings must display actual antialiasing status'
    text=c.create_unicode_buffer(256)
    send(label,0xD,len(text),c.addressof(text)) # WM_GETTEXT marshals across processes.
    return text.value
def tray_window():
    result=[]
    @CALLBACK
    def visit(hwnd,_):
        pid=w.DWORD();user.GetWindowThreadProcessId(hwnd,c.byref(pid))
        if pid.value==process.pid and class_name(hwnd)=='BaCafeNativeTray':result.append(hwnd)
        return True
    user.EnumWindows(visit,0)
    return result
class NotifyIcon(c.Structure):
    _fields_=[('cbSize',w.DWORD),('hWnd',w.HWND),('uID',w.UINT),('uFlags',w.UINT),('uCallbackMessage',w.UINT),('hIcon',w.HICON),('szTip',w.WCHAR*128),('dwState',w.DWORD),('dwStateMask',w.DWORD),('szInfo',w.WCHAR*256),('uVersion',w.UINT),('szInfoTitle',w.WCHAR*64),('dwInfoFlags',w.DWORD),('guidItem',c.c_byte*16),('hBalloonIcon',w.HICON)]
shell=c.WinDLL('shell32');shell.Shell_NotifyIconW.argtypes=[w.DWORD,c.POINTER(NotifyIcon)]
def registered(tray):
    data=NotifyIcon();data.cbSize=c.sizeof(data);data.hWnd=tray;data.uID=1
    return shell.Shell_NotifyIconW(1,c.byref(data)) # Modify no attributes; verify the shell owns this icon.
try:
    full=wait_for(lambda: windows('BaCafeNativeSettings'),'settings')[0]
    pet=wait_for(lambda: windows('BaCafeNativeInput'),'pet rendered')[0]
    wait_for(lambda: sampling_text().startswith('已生效：'),'initial GPU sampling status')
    for quality,scale in ((0,'1×'),(1,'1.5×'),(2,'2×')):
        combo=user.GetDlgItem(panel(),101)
        send(combo,0x14E,quality) # CB_SETCURSEL
        send(panel(),0x111,101|(1<<16),combo) # CBN_SELCHANGE
        status=wait_for(lambda: (text if (text:=sampling_text()).startswith('已生效：'+scale+' 超采样') else None),'changed quality reflected after GPU rendering')
        if quality==0:assert 'MSAA 关闭' in status
        if quality==2:assert '4× MSAA' in status or '自动降低采样' in status
        print('Actual rendered quality:',status.replace('\n',' / '),flush=True)
    send(pet,0x20A,120<<16)
    zoom=read_roster()['characters'][0]['zoom'];assert zoom>1,'Model size changed before hiding'
    send(full,0x111,1003)
    user.PostMessageW(panel(),0x111,2100,0)
    wait_for(lambda: not windows('BaCafeNativePet') or windows('#32770'),'hide last pet')
    assert not windows('#32770'),'Hiding the final model must not prompt to exit'
    assert process.poll() is None and not any(row['active'] for row in read_roster()['characters'])
    send(full,0x111,1000)
    assert '已隐藏' in sampling_text(),'All-hidden state must not claim that MSAA is rendering'
    send(full,0x111,1003)
    tray=wait_for(tray_window,'resident tray icon window')[0]
    assert registered(tray),'The icon is registered with the Windows notification area'
    missing=NotifyIcon();missing.cbSize=c.sizeof(missing);missing.hWnd=tray;missing.uID=1
    assert shell.Shell_NotifyIconW(2,c.byref(missing)),'Remove this isolated test icon to simulate Explorer recreation'
    user.RegisterWindowMessageW.argtypes=[w.LPCWSTR]
    user.PostMessageW(tray,user.RegisterWindowMessageW('TaskbarCreated'),0,0);time.sleep(.2)
    assert registered(tray),'Taskbar recreation keeps the notification icon available'
    send(full,0x10)
    user.PostMessageW(tray,0x8000+61,0,(1<<16)|0x400)
    full=wait_for(lambda: windows('BaCafeNativeSettings'),'tray click opens full settings')[0]
    assert not windows('BaCafeNativePet')
    user.PostMessageW(tray,0x8000+61,0,(1<<16)|0x7B)
    quick=wait_for(lambda: windows('BaCafeQuickMenu'),'tray context menu')[0]
    compact=user.GetDlgItem(quick,3200)
    send(compact,0x186,0);send(compact,0x100,0x20)
    wait_for(lambda: windows('BaCafeNativePet'),'show pet from tray roster')
    wait_for(lambda: user.IsWindowEnabled(user.GetDlgItem(quick,3000)),'restored pet loaded')
    send(quick,0x111,3011)
    send(full,0x111,1000)
    mode=user.GetDlgItem(panel(),100)
    assert send(mode,0x146)>=3,'Formation is available in the shipped full-interface mode combo'
    send(mode,0x14E,2);send(panel(),0x111,(1<<16)|100)
    wait_for(lambda: read_roster()['characters'][0]['mode']=='formation','save the shipped formation mode')
    assert not user.IsWindowEnabled(user.GetDlgItem(panel(),105)),'Shipped formation disables follow controls'
    pet=wait_for(lambda: windows('BaCafeNativeInput'),'formation input window')[0]
    for next_mode,index in (('cafe',0),('battle',1),('formation',2)):
        for _ in range(3):
            send(pet,0x201,1);send(pet,0x202);time.sleep(.025)
        wait_for(lambda: read_roster()['characters'][0]['mode']==next_mode,'triple-click mode persisted: '+next_mode)
        wait_for(lambda: send(user.GetDlgItem(panel(),100),0x147)==index,'full interface follows gesture mode: '+next_mode)
    before_zoom=read_roster()['characters'][0]['zoom'];send(pet,0x20A,120<<16)
    wait_for(lambda: read_roster()['characters'][0]['zoom']>before_zoom,'formation wheel size saved')
    zoom=read_roster()['characters'][0]['zoom']
    assert read_roster()['characters'][0]['mode']=='formation','Scaling formation does not switch modes'
    send(full,0x111,1003);send(panel(),0x111,2100)
    wait_for(lambda: not windows('BaCafeNativePet'),'hide model again')
    send(full,0x111,2);assert process.wait(timeout=10)==0
    process=subprocess.Popen([str(PROGRAM/'BA-Cafe-Pets.exe'),'--data-dir',str(SCRATCH),'--settings'],cwd=PROGRAM,env=environment)
    full=wait_for(lambda: windows('BaCafeNativeSettings'),'restart with all models hidden')[0]
    wait_for(lambda: user.IsWindowEnabled(user.GetDlgItem(full,2200)),'imports available in background')
    assert tray_window() and not windows('BaCafeNativePet') and not any(row['active'] for row in read_roster()['characters'])
    assert abs(read_roster()['characters'][0]['zoom']-round(zoom,2))<.0001,'Background restart preserves the hidden model size'
    assert read_roster()['characters'][0]['mode']=='formation','Background restart preserves formation mode'
    send(full,0x111,1003);send(panel(),0x111,2100)
    wait_for(lambda: windows('BaCafeNativePet'),'restore a formation pet after restart')
    send(full,0x111,1000)
    wait_for(lambda: send(user.GetDlgItem(panel(),100),0x147)==2,'restored character actually enters formation')
    send(full,0x111,1003);send(panel(),0x111,2100)
    wait_for(lambda: not windows('BaCafeNativePet'),'hide restored formation pet')
    send(full,0x111,1003)
    user.PostMessageW(panel(),0x111,2102,0)
    confirm=wait_for(lambda: windows('#32770'),'remove final model record confirmation')[0]
    user.PostMessageW(confirm,0x111,6,0)
    wait_for(lambda: len(read_roster()['characters'])==0,'empty model library')
    assert process.poll() is None and tray_window() and (PROGRAM/'model/Shiroko.glb').is_file()
    send(full,0x111,2);assert process.wait(timeout=10)==0
    process=subprocess.Popen([str(PROGRAM/'BA-Cafe-Pets.exe'),'--data-dir',str(SCRATCH),'--settings'],cwd=PROGRAM,env=environment)
    full=wait_for(lambda: windows('BaCafeNativeSettings'),'restart after removing all models')[0]
    wait_for(lambda: user.IsWindowEnabled(user.GetDlgItem(full,2200)),'empty-library imports available')
    assert not read_roster()['characters'] and not windows('BaCafeNativePet') and tray_window()
    user.PostMessageW(tray_window()[0],0x8000+61,0,(1<<16)|0x7B)
    quick=wait_for(lambda: windows('BaCafeQuickMenu'),'empty-library tray exit menu')[0]
    send(quick,0x111,3012);assert process.wait(timeout=10)==0
    print('Tray integration passed: hide last model, tray menu, full triple-click mode cycle, saved formation wheel size, persistent all-hidden state, restore model, empty library, and explicit exit.')
finally:
    if process.poll() is None:
        for dialog in windows('#32770'):user.PostMessageW(dialog,0x111,2,0)
        for hwnd in windows('BaCafeNativeSettings'):user.PostMessageW(hwnd,0x111,2,0)
        try:process.wait(timeout=8)
        except subprocess.TimeoutExpired:process.terminate()
