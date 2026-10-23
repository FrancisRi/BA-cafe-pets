import ctypes as c
from ctypes import wintypes as w
import json,os,shutil,subprocess,sys,time,uuid
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'artifacts/model-storage'/('compat-'+uuid.uuid4().hex)
program=WORK/'application'
program.mkdir(parents=True)
launcher=program/'BA-Cafe-Pets.exe'
shutil.copyfile(Path(sys.argv[1]).resolve(),launcher)
environment=os.environ.copy()
environment['BA_NATIVE_HOME']=str(WORK/'profile')
environment.pop('BA_NATIVE_DATA_DIR',None)
assert subprocess.run([str(launcher),'--prepare-only'],env=environment,timeout=30).returncode==0
cache=next((WORK/'profile/app').iterdir())
legacy=program/'build/bin/data'
(legacy/'models').mkdir(parents=True)
def key(path):
    value=14695981039346656037
    for ch in str(path).lower(): value=((value^ord(ch))*1099511628211)&((1<<64)-1)
    return f'{value:x}'
rows=[]
for index in range(2):
    file=legacy/'models'/f'CompatibilityModel{index}.glb'
    shutil.copyfile(ROOT/'assets/Shiroko.glb',file)
    rows.append(dict(id=key(file),path=str(file),name=f'CompatibilityModel{index}',active=index==0,mode='battle' if index==0 else 'cafe',zoom=0.87))
(legacy/'characters.json').write_text(json.dumps(dict(characters=rows,selected=rows[0]['id'])),encoding='utf-8')
legacy_bytes=(legacy/'characters.json').read_bytes()
user=c.WinDLL('user32',use_last_error=True)
kernel=c.WinDLL('kernel32',use_last_error=True)
CALLBACK=c.WINFUNCTYPE(w.BOOL,w.HWND,w.LPARAM)
user.EnumWindows.argtypes=[CALLBACK,w.LPARAM]
user.GetWindowThreadProcessId.argtypes=[w.HWND,c.POINTER(w.DWORD)]
user.GetClassNameW.argtypes=[w.HWND,w.LPWSTR,c.c_int]
user.IsWindowVisible.argtypes=[w.HWND]
user.PostMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM]
user.GetWindowLongPtrW.argtypes=[w.HWND,c.c_int]
user.GetWindowLongPtrW.restype=c.c_ssize_t
kernel.OpenProcess.argtypes=[w.DWORD,w.BOOL,w.DWORD]
kernel.OpenProcess.restype=w.HANDLE
kernel.QueryFullProcessImageNameW.argtypes=[w.HANDLE,w.DWORD,w.LPWSTR,c.POINTER(w.DWORD)]
kernel.CloseHandle.argtypes=[w.HANDLE]

def owner_path(pid):
    handle=kernel.OpenProcess(0x1000,False,pid)
    if not handle:
        return None
    try:
        text=c.create_unicode_buffer(32768)
        length=w.DWORD(len(text))
        return Path(text.value) if kernel.QueryFullProcessImageNameW(handle,0,text,c.byref(length)) else None
    finally:
        kernel.CloseHandle(handle)

def windows(expected):
    found={}
    @CALLBACK
    def visit(hwnd,_):
        cls=c.create_unicode_buffer(128)
        user.GetClassNameW(hwnd,cls,128)
        if cls.value in ('BaCafeNativeSettings','BaCafeNativeInput','BaCafeNativePet') and user.IsWindowVisible(hwnd):
            pid=w.DWORD()
            user.GetWindowThreadProcessId(hwnd,c.byref(pid))
            path=owner_path(pid.value)
            if path and str(path).lower()==str(expected).lower():
                found[cls.value]=hwnd
        return True
    user.EnumWindows(visit,0)
    return found

def smoke(command,expected,env,label):
    process=subprocess.Popen(command,cwd=expected.parent,env=env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    current={}
    try:
        deadline=time.monotonic()+30
        while time.monotonic()<deadline:
            assert process.poll() is None, f'{label} exited: {process.returncode}'
            current=windows(expected)
            if {'BaCafeNativeSettings','BaCafeNativeInput','BaCafeNativePet'}<=set(current):
                break
            time.sleep(0.1)
        else:
            raise RuntimeError(label+' did not display the default pet/settings')
        assert not user.GetWindowLongPtrW(current['BaCafeNativeSettings'],-20)&8
        assert user.GetWindowLongPtrW(current['BaCafeNativeInput'],-20)&8
        assert user.GetWindowLongPtrW(current['BaCafeNativePet'],-20)&8
        user.PostMessageW(current['BaCafeNativeSettings'],0x111,2,0)
        assert process.wait(timeout=12)==0
        print(label+' startup, default rendering, window order and clean exit passed.',flush=True)
    finally:
        if process.poll() is None:
            for hwnd in windows(expected).values():
                user.PostMessageW(hwnd,0x111,2,0)
            try:
                process.wait(timeout=8)
            except subprocess.TimeoutExpired:
                process.terminate()


smoke([str(launcher),'--settings'],cache/'ba_cafe_lite.exe',environment,'Launcher compatibility')
current=json.loads((WORK/'profile/data/characters.json').read_text(encoding='utf-8'))
found={row['name']:row for row in current['characters']}
assert {'CompatibilityModel0','CompatibilityModel1'}<=set(found), 'Legacy models missing: '+str(list(found))
assert found['CompatibilityModel0']['active'] and found['CompatibilityModel0']['mode']=='battle'
assert abs(found['CompatibilityModel0']['zoom']-0.87)<0.001
assert (legacy/'characters.json').read_bytes()==legacy_bytes
assert all(Path(row['path']).is_file() for row in rows)
print('Legacy developer models recovered by standalone launcher, with mode/visibility/zoom and original files preserved.')
