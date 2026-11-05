# Copyright (C) 2026 FrancisRi
# SPDX-License-Identifier: GPL-3.0-only
"""Reproduce a portable installation with imported models but no packaged seed."""
from pathlib import Path
from ctypes import wintypes as w
import argparse,ctypes as c,json,os,shutil,subprocess,time,uuid,zipfile

ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
parser.add_argument('package',type=Path)
parser.add_argument('--core',type=Path)
parser.add_argument('--expect-error',action='store_true')
parser.add_argument('--empty',action='store_true',help='Start after removing the last model, then add a new one')
args=parser.parse_args()
WORK=ROOT/'artifacts/storage-startup'/uuid.uuid4().hex
PROGRAM=WORK/'application';PROFILE=PROGRAM/'data'
with zipfile.ZipFile(args.package.resolve()) as bundle:bundle.extractall(PROGRAM)
if args.core:shutil.copyfile(args.core.resolve(),PROGRAM/'BA-Cafe-Pets.exe')
seed=PROGRAM/'model/Shiroko.glb'
assert seed.resolve().is_relative_to(WORK.resolve())
seed.unlink() # Only the disposable fixture's bundled model is removed.
model=PROGRAM/'model/Imported.glb';shutil.copyfile(ROOT/'assets/Shiroko.glb',model)
value=14695981039346656037
for ch in str(model).lower():value=((value^ord(ch))*1099511628211)&((1<<64)-1)
id=f'{value:x}'
roster={'characters':[{'id':id,'name':'Imported','path':str(model),'mode':'cafe','active':True,'zoom':1.24}],'selected':id}
if args.empty:model.unlink();roster={'characters':[],'selected':''}
PROFILE.mkdir()
(PROFILE/'characters.json').write_text(json.dumps(roster),encoding='utf-8')
(PROFILE/'model-library.json').write_text(json.dumps({'directory':'','previousDirectories':[],'recoveredProfiles':[],'bundledModel':'model\\Shiroko.glb','unifiedLayout':True}),encoding='utf-8')
environment=os.environ.copy()
for key in ('BA_NATIVE_PROGRAM_DIR','BA_NATIVE_DATA_DIR','BA_NATIVE_HOME'):environment.pop(key,None)
u=c.WinDLL('user32');callback=c.WINFUNCTYPE(w.BOOL,w.HWND,w.LPARAM)
u.EnumWindows.argtypes=[callback,w.LPARAM];u.GetClassNameW.argtypes=[w.HWND,w.LPWSTR,c.c_int]
u.GetWindowThreadProcessId.argtypes=[w.HWND,c.POINTER(w.DWORD)];u.GetDlgItem.argtypes=[w.HWND,c.c_int];u.GetDlgItem.restype=w.HWND
u.IsWindowVisible.argtypes=[w.HWND];u.IsWindowEnabled.argtypes=[w.HWND];u.PostMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM]
u.SendMessageTimeoutW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM,w.UINT,w.UINT,c.POINTER(c.c_size_t)]
process=None
def windows(cls,visible=False):
 found=[]
 @callback
 def visit(hwnd,_):
  pid=w.DWORD();u.GetWindowThreadProcessId(hwnd,c.byref(pid))
  if pid.value==process.pid:
   name=c.create_unicode_buffer(128);u.GetClassNameW(hwnd,name,128)
   if name.value==cls and (not visible or u.IsWindowVisible(hwnd)):found.append(hwnd)
  return True
 u.EnumWindows(visit,0);return found
def wait(check,label):
 deadline=time.monotonic()+25
 while time.monotonic()<deadline:
  assert process.poll() is None,'Unexpected process exit: '+label
  result=check()
  if result:return result
  time.sleep(.05)
 raise RuntimeError('Timed out: '+label)
def status(menu):
 child=u.GetDlgItem(menu,3102)
 if not child:return None
 value=c.create_unicode_buffer(4096);result=c.c_size_t()
 assert u.SendMessageTimeoutW(child,0xD,4096,c.addressof(value),2,1000,c.byref(result))
 return value.value
for restart in range(2):
 process=subprocess.Popen([str(PROGRAM/'BA-Cafe-Pets.exe'),'--settings','--diagnostics'],cwd=PROGRAM,env=environment,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
 try:
  if not args.empty:wait(lambda:windows('BaCafeNativeInput',True),'Imported model renders')
  else:
   full=wait(lambda:windows('BaCafeNativeSettings',True),'Empty model library settings')[0]
   wait(lambda:u.IsWindowEnabled(u.GetDlgItem(full,2200)),'Empty library import button is available')
  tray=wait(lambda:windows('BaCafeNativeTray'),'Tray initialized')[0]
  u.PostMessageW(tray,0x8000+61,0,(1<<16)|0x7B)
  menu=wait(lambda:next(iter(windows('BaCafeQuickMenu',True)),None),'Tray quick menu opens')
  text=status(menu);failed='模型目录统一失败' in text
  assert failed==args.expect_error,repr(text)
  print(json.dumps({'restart':restart,'status':text,'expected_error':args.expect_error},ensure_ascii=False),flush=True)
  assert model.is_file()!=args.empty and not seed.exists()
  if args.empty and restart==1:
   shutil.copyfile(ROOT/'assets/Shiroko.glb',model)
   u.PostMessageW(tray,0x8000+61,0,(1<<16)|0x7B)
   listbox=u.GetDlgItem(menu,3200);result=c.c_size_t()
   def count():
    u.SendMessageTimeoutW(listbox,0x18B,0,0,2,1000,c.byref(result));return result.value==1
   wait(count,'New GLB discovered after deleting all models')
   u.SendMessageTimeoutW(listbox,0x186,0,0,2,1000,c.byref(result))
   u.PostMessageW(listbox,0x100,0x20,0)
   wait(lambda:windows('BaCafeNativeInput',True),'New model loads from the empty host')
  u.PostMessageW(tray,0x10,0,0);assert process.wait(timeout=12)==0
  saved=json.loads((PROFILE/'characters.json').read_text(encoding='utf-8'))
  if args.empty and restart==0:assert not saved['characters']
  else:assert saved['selected']==id and len(saved['characters'])==1 and saved['characters'][0]['active']
 finally:
  if process.poll() is None:
   for hwnd in windows('BaCafeNativeTray'):u.PostMessageW(hwnd,0x10,0,0)
   try:process.wait(timeout=8)
   except subprocess.TimeoutExpired:process.terminate();process.wait(timeout=5)
print('Portable missing-seed startup, tray status, imported model and restart checks passed.',flush=True)
