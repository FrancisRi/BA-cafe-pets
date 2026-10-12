"""Check real OS mouse delivery, independent of WindowFromPoint's result.

Only synthetic input over our own two test processes is permitted. The cursor
is restored on exit. Pillow is a test dependency, never a release dependency.
"""
import ctypes as c, pathlib, subprocess, time
from PIL import ImageGrab
u=c.windll.user32
u.SetProcessDpiAwarenessContext(c.c_void_p(-4))
H=c.c_void_p; L=c.c_ssize_t
CALL=c.WINFUNCTYPE(L,H,c.c_uint,c.c_size_t,L)
u.DefWindowProcW.argtypes=[H,c.c_uint,c.c_size_t,L];u.DefWindowProcW.restype=L
clicks=[]
def proc(h,m,w,l):
 if m==0x201:clicks.append(True)
 if m==0x21:return 3
 return u.DefWindowProcW(h,m,w,l)
cb=CALL(proc)
class WC(c.Structure):_fields_=[('style',c.c_uint),('proc',CALL),('ce',c.c_int),('we',c.c_int),('instance',H),('icon',H),('cursor',H),('brush',H),('menu',c.c_wchar_p),('name',c.c_wchar_p)]
class Point(c.Structure):_fields_=[('x',c.c_long),('y',c.c_long)]
class Rect(c.Structure):_fields_=[('left',c.c_long),('top',c.c_long),('right',c.c_long),('bottom',c.c_long)]
class Msg(c.Structure):_fields_=[('hwnd',H),('message',c.c_uint),('w',c.c_size_t),('l',L),('time',c.c_uint),('pt',Point)]
u.CreateWindowExW.restype=H;u.CreateWindowExW.argtypes=[c.c_uint,c.c_wchar_p,c.c_wchar_p,c.c_uint,c.c_int,c.c_int,c.c_int,c.c_int,H,H,H,H]
u.GetWindowLongPtrW.argtypes=[H,c.c_int];u.GetWindowLongPtrW.restype=L
u.SetWindowLongPtrW.argtypes=[H,c.c_int,L]
u.SetLayeredWindowAttributes.argtypes=[H,c.c_uint,c.c_ubyte,c.c_uint]
u.SetWindowPos.argtypes=[H,H,c.c_int,c.c_int,c.c_int,c.c_int,c.c_uint]
u.DestroyWindow.argtypes=[H];u.GetWindowRect.argtypes=[H,c.POINTER(Rect)]
u.WindowFromPoint.argtypes=[Point];u.WindowFromPoint.restype=H
project=pathlib.Path(__file__).resolve().parent.parent
root=project/'artifacts/three-input-check'
log=open(root/'probe.log','w')
p=subprocess.Popen([str(root/'desktop_pet_three.exe'),'--model',str(project/'assets/Hibiki.glb'),'--clip','Cafe_Idle','--sample','1','--duration','20'],cwd=root,stdout=log,stderr=log,creationflags=0x08000000)
windows=[]
ENUM=c.WINFUNCTYPE(c.c_int,H,L)
def enum(h,l):
 pid=c.c_ulong();u.GetWindowThreadProcessId(h,c.byref(pid))
 if pid.value==p.pid:
  name=c.create_unicode_buffer(200);u.GetClassNameW(h,name,200)
  if name.value in ['DesktopPetThree','DesktopPetThreeInput']:windows.append((name.value,h))
 return 1
u.GetWindowThreadProcessId.argtypes=[H,c.POINTER(c.c_ulong)];u.GetClassNameW.argtypes=[H,c.c_wchar_p,c.c_int]
bg=None;old=Point();u.GetCursorPos(c.byref(old))
def pump():
 msg=Msg()
 while u.PeekMessageW(c.byref(msg),None,0,0,1):u.TranslateMessage(c.byref(msg));u.DispatchMessageW(c.byref(msg))
def wait(t):
 end=time.time()+t
 while time.time()<end:pump();time.sleep(.005)
try:
 deadline=time.time()+12
 while 'READY Hibiki:' not in (root/'probe.log').read_text(errors='ignore'):
  assert time.time()<deadline,'Preview failed to load';time.sleep(.1)
 u.EnumWindows(ENUM(enum),0)
 render=dict(windows)['DesktopPetThree'];inp=dict(windows)['DesktopPetThreeInput'];r=Rect();u.GetWindowRect(render,c.byref(r))
 g=c.windll.gdi32;g.CreateRectRgn.argtypes=[c.c_int]*4;g.CreateRectRgn.restype=H
 u.GetWindowRgn.argtypes=[H,H];g.GetRgnBox.argtypes=[H,c.POINTER(Rect)];g.PtInRegion.argtypes=[H,c.c_int,c.c_int];g.DeleteObject.argtypes=[H]
 region=g.CreateRectRgn(0,0,0,0);bounds=Rect();deadline=time.time()+8
 while True:
  u.GetWindowRgn(inp,region);g.GetRgnBox(region,c.byref(bounds))
  if bounds.right>bounds.left and bounds.bottom>bounds.top:break
  assert time.time()<deadline,'Alpha input region did not arrive';wait(.05)
 g.DeleteObject(region)
 assert bounds.right>bounds.left and bounds.bottom>bounds.top,'Alpha input region did not arrive'
 # Capture only our own white test window and the fixed review character above it.
 x=r.left+bounds.left-16;y=r.top+bounds.top-16;width=bounds.right-bounds.left+32;height=bounds.bottom-bounds.top+32
 wc=WC();wc.proc=cb;wc.name='PetClickReceiver';wc.brush=H(6);u.RegisterClassW(c.byref(wc))
 bg=u.CreateWindowExW(0x08000008,wc.name,'Desktop pet input test',0x90000000,x,y,width,height,None,None,None,None)
 u.SetWindowPos(render,H(-1),0,0,0,0,0x13);u.SetWindowPos(inp,H(-1),0,0,0,0,0x13);wait(.2)
 before=ImageGrab.grab(bbox=(x,y,x+width,y+height),all_screens=True).convert('RGB')
 model_pixels=sum(1 for px in before.get_flattened_data() if min(px)<240)
 assert model_pixels>500,'Layered display did not render the model over the test background'
 assert u.GetWindowLongPtrW(render,-20)&0x80020==0x80020,'Display needs layered + transparent styles'
 blank=Point(x+4,y+4)
 # Never click any window belonging to the user.
 assert u.WindowFromPoint(blank)==bg,'Background probe must be in our own test window'
 start=len(clicks);u.SetCursorPos(blank.x,blank.y);u.mouse_event(2,0,0,0,0);u.mouse_event(4,0,0,0,0);wait(.15)
 assert len(clicks)==start+1,'Actual mouse input was swallowed by the other-process display window'
 # Conversely, alpha-covered points must reach the pet input window.
 region=g.CreateRectRgn(0,0,0,0);u.GetWindowRgn(inp,region)
 body=None
 for yy in range(bounds.top,bounds.bottom,4):
  for xx in range(bounds.left,bounds.right,4):
   point=Point(r.left+xx,r.top+yy)
   if g.PtInRegion(region,xx,yy) and u.WindowFromPoint(point)==inp:body=point;break
  if body:break
 g.DeleteObject(region);assert body,'No alpha-covered input point was found'
 class GUI(c.Structure):_fields_=[('size',c.c_uint),('flags',c.c_uint),('active',H),('focus',H),('capture',H),('menu',H),('move',H),('caret',H),('rect',Rect)]
 owner=c.c_ulong();thread=u.GetWindowThreadProcessId(inp,c.byref(owner));gui=GUI();gui.size=c.sizeof(gui)
 u.SetCursorPos(body.x,body.y);u.mouse_event(8,0,0,0,0);wait(.1)
 u.GetGUIThreadInfo(thread,c.byref(gui));captured=gui.capture==inp
 u.mouse_event(16,0,0,0,0);wait(.1)
 assert captured,'Actual right-button input must begin capture in the pet input window'
 assert len(clicks)==start+1,'Clicking the pet must not click the background'
 print('PASS: model visible (%d colored pixels), real cross-process background click, real pet capture, cursor restored'%model_pixels,flush=True)
finally:
 u.SetCursorPos(old.x,old.y)
 if bg:u.DestroyWindow(bg)
 if p.poll() is None:p.terminate()
 p.wait();log.close()
