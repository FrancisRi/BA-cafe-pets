"""Exercise shipped packages with a fresh, all-hidden model library on Windows."""
from pathlib import Path
from ctypes import wintypes as w
import argparse, ctypes as c, json, os, shutil, subprocess, time, uuid, zipfile

ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
parser.add_argument('package',type=Path)
parser.add_argument('--case',choices=('menu','models','both'),default='both')
parser.add_argument('--core',type=Path,help='Test a rebuilt native core using ZIP resources')
args=parser.parse_args()
WORK=ROOT/'artifacts/tray'/('background-'+uuid.uuid4().hex)
PROGRAM=WORK/'application';PROGRAM.mkdir(parents=True)
environment=os.environ.copy()
for name in ('BA_NATIVE_PROGRAM_DIR','BA_NATIVE_DATA_DIR','BA_NATIVE_HOME'):environment.pop(name,None)
environment['BA_NATIVE_HOME']=str(WORK/'home')
if args.package.suffix.lower()=='.zip':
    with zipfile.ZipFile(args.package.resolve()) as archive:archive.extractall(PROGRAM)
    executable=core=PROGRAM/'BA-Cafe-Pets.exe'
    if args.core:shutil.copyfile(args.core.resolve(),core)
else:
    assert args.core is None,'A single-file release must be tested with its own embedded core'
    executable=PROGRAM/'BA-Cafe-Pets.exe';shutil.copyfile(args.package.resolve(),executable)
    subprocess.run([str(executable),'--prepare-only'],env=environment,check=True,timeout=40)
    core=next((WORK/'home/app').glob('*/BA-Cafe-Pets.exe'))

def key(path):
    value=14695981039346656037
    for ch in str(path).lower():value=((value^ord(ch))*1099511628211)&((1<<64)-1)
    return f'{value:x}'
rows=[]
for name in ('Previous','OtherB','OtherC'):
    file=PROGRAM/'model'/(name+'.glb');file.parent.mkdir(exist_ok=True)
    shutil.copyfile(ROOT/'assets/Shiroko.glb',file)
    rows.append(dict(id=key(file),name=name,path=str(file),active=False,mode='cafe',zoom=1))

user=c.WinDLL('user32',use_last_error=True);kernel=c.WinDLL('kernel32',use_last_error=True)
CALLBACK=c.WINFUNCTYPE(w.BOOL,w.HWND,w.LPARAM)
user.EnumWindows.argtypes=[CALLBACK,w.LPARAM]
user.EnumChildWindows.argtypes=[w.HWND,CALLBACK,w.LPARAM]
user.GetClassNameW.argtypes=[w.HWND,w.LPWSTR,c.c_int]
user.GetWindowThreadProcessId.argtypes=[w.HWND,c.POINTER(w.DWORD)]
user.IsWindowVisible.argtypes=[w.HWND];user.IsWindowEnabled.argtypes=[w.HWND]
user.GetDlgItem.argtypes=[w.HWND,c.c_int];user.GetDlgItem.restype=w.HWND
user.SendMessageTimeoutW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM,w.UINT,w.UINT,c.POINTER(c.c_size_t)]
kernel.OpenProcess.argtypes=[w.DWORD,w.BOOL,w.DWORD];kernel.OpenProcess.restype=w.HANDLE
kernel.QueryFullProcessImageNameW.argtypes=[w.HANDLE,w.DWORD,w.LPWSTR,c.POINTER(w.DWORD)]
kernel.CloseHandle.argtypes=[w.HANDLE]

def class_name(hwnd):
    name=c.create_unicode_buffer(128);user.GetClassNameW(hwnd,name,len(name));return name.value
def windows(cls,visible=True):
    found=[]
    @CALLBACK
    def visit(hwnd,_):
        if class_name(hwnd)!=cls or (visible and not user.IsWindowVisible(hwnd)):return True
        pid=w.DWORD();user.GetWindowThreadProcessId(hwnd,c.byref(pid));handle=kernel.OpenProcess(0x1000,False,pid.value)
        if handle:
            path=c.create_unicode_buffer(32768);length=w.DWORD(len(path))
            if kernel.QueryFullProcessImageNameW(handle,0,path,c.byref(length)) and Path(path.value)==core:found.append(hwnd)
            kernel.CloseHandle(handle)
        return True
    user.EnumWindows(visit,0);return found
def send(hwnd,msg,wp=0,lp=0):
    result=c.c_size_t()
    assert user.SendMessageTimeoutW(hwnd,msg,wp,lp,2,3000,c.byref(result)),f'Message timeout {msg:#x}'
    return result.value
def text(hwnd):
    value=c.create_unicode_buffer(256);send(hwnd,0xD,len(value),c.addressof(value));return value.value
def panel(full):
    found=[]
    @CALLBACK
    def visit(hwnd,_):
        if class_name(hwnd)=='BaCafeNativeSettings':found.append(hwnd)
        return True
    user.EnumChildWindows(full,visit,0);return found[0]
def wait(check,label,seconds=10):
    deadline=time.monotonic()+seconds
    while time.monotonic()<deadline:
        assert process.poll() is None,'Application exited: '+label
        if result:=check():return result
        time.sleep(.05)
    raise AssertionError(label+'; status='+text(user.GetDlgItem(full,2300)))
def notify(tray,event):send(tray,0x803D,0,(1<<16)|event)

failures=[]
for case in (('menu','models') if args.case=='both' else (args.case,)):
    profile=WORK/case;profile.mkdir()
    (profile/'characters.json').write_text(json.dumps(dict(selected=rows[0]['id'],characters=rows)),encoding='utf-8')
    log=open(WORK/(case+'.log'),'w',encoding='utf-8')
    process=subprocess.Popen([str(executable),'--data-dir',str(profile),'--settings','--diagnostics'],env=environment,cwd=PROGRAM,stdout=log,stderr=log)
    full=None
    try:
        full=wait(lambda: windows('BaCafeNativeSettings'),'Initial settings')[0]
        wait(lambda: user.IsWindowEnabled(user.GetDlgItem(full,2200)),'Hidden host finished initialization')
        assert not windows('BaCafeNativePet'),'All-hidden launch must stay hidden'
        # Let the host enter its idle wait before selecting another model.
        time.sleep(.6)
        tray=wait(lambda: windows('BaCafeNativeTray',False),'Tray host')[0]
        if case=='menu':
            notify(tray,0x205) # Raw WM_RBUTTONUP may precede the version-4 context notification.
            notify(tray,0x7B) # WM_CONTEXTMENU is the semantic notification.
            time.sleep(.2)
            quick=windows('BaCafeQuickMenu')
            assert quick,'Tray right-click notifications must leave the flyout open'
            quick=quick[0];notify(tray,0x7B);time.sleep(.2)
            assert user.IsWindowVisible(quick),'Repeated context notifications must not toggle the flyout closed'
            assert send(user.GetDlgItem(quick,3200),0x18B)==3,'All imported choices remain accessible'
            send(quick,0x111,3011)
            assert user.IsWindowVisible(full) and not user.IsWindowVisible(quick),'Flyout opens the full interface'
            notify(tray,0x7B);quick=wait(lambda: windows('BaCafeQuickMenu'),'Reopen tray menu')[0]
            send(quick,0x6,0,full) # WM_ACTIVATE / WA_INACTIVE still dismisses the flyout.
            assert not user.IsWindowVisible(quick),'Flyout still dismisses on deactivation'
        else:
            send(full,0x111,1003);page=panel(full);listing=user.GetDlgItem(page,2000)
            send(listing,0x186,1);send(page,0x111,2000|(1<<16),listing);send(page,0x111,2100)
            wait(lambda: windows('BaCafeNativePet'),'A different model loads from an entirely hidden startup',6)
            wait(lambda: 'READY OtherB:' in (profile/'logs/pet.log').read_text(encoding='utf-8',errors='replace'),'OtherB finishes loading')
            assert 'OtherB' in text(user.GetDlgItem(full,11)),'The chosen model becomes current'
            send(page,0x111,2100);wait(lambda: not windows('BaCafeNativePet'),'Hide OtherB');time.sleep(.6)
            notify(tray,0x7B);quick=wait(lambda: windows('BaCafeQuickMenu'),'Select another model from tray')[0]
            compact=user.GetDlgItem(quick,3200)
            for index in (1,2):send(compact,0x186,index);send(compact,0x100,0x20)
            wait(lambda: len(windows('BaCafeNativePet'))==2,'Two queued models load from the tray while the host stays hidden',6)
            wait(lambda: user.IsWindowEnabled(user.GetDlgItem(quick,3000)),'OtherC load completed')
            wait(lambda: all('READY '+row['name']+':' in (profile/'logs/pet.log').read_text(encoding='utf-8',errors='replace') for row in rows),'All three models finish initialization')
            send(quick,0x111,3011)
            assert 'OtherC' in text(user.GetDlgItem(full,11))
            saved=json.loads((profile/'characters.json').read_text())
            assert saved['selected']==rows[2]['id'] and sum(row['active'] for row in saved['characters'])==2
        print(case+': passed',flush=True)
    except Exception as error:
        failures.append(case+': '+str(error));print(failures[-1],flush=True)
    finally:
        for hwnd in windows('BaCafeNativeSettings',False):send(hwnd,0x111,2)
        try:process.wait(timeout=10)
        except subprocess.TimeoutExpired:process.terminate();process.wait(timeout=5)
        log.close()
print('Isolated artifacts:',WORK,flush=True)
if failures:raise SystemExit(1)
