param([ValidateSet('raylib','three')][string]$Renderer='raylib', [string]$Model='', [string]$Behavior='', [switch]$Smooth)
$ErrorActionPreference='Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
if(!$Model){$Model=if($Renderer -eq 'three'){'assets/Azusa.glb'}else{'artifacts/imports/Azusa-0f0b09f0b87b/Azusa.glb'}}
if(!$Behavior){$Behavior='artifacts/imports/Azusa-0f0b09f0b87b/behavior.bt.xml'}
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class BattleNativeTest {
 public static string PetClass="RaylibDesktopPet";
 public delegate bool EnumProc(IntPtr window,IntPtr data);
 [DllImport("user32.dll")]public static extern bool EnumWindows(EnumProc callback,IntPtr data);
 [DllImport("user32.dll")]public static extern uint GetWindowThreadProcessId(IntPtr window,out uint process);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern int GetClassName(IntPtr window,System.Text.StringBuilder text,int size);
 [DllImport("user32.dll")]public static extern bool PostMessage(IntPtr window,uint message,IntPtr w,IntPtr l);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern IntPtr CreateWindowEx(uint ex,string cls,string title,uint style,int x,int y,int width,int height,IntPtr parent,IntPtr menu,IntPtr instance,IntPtr param);
 [DllImport("user32.dll")]public static extern bool DestroyWindow(IntPtr window);
 [DllImport("user32.dll",EntryPoint="GetWindowRect")]private static extern bool NativeWindowRect(IntPtr window,out Rect rect);
 [DllImport("user32.dll")]private static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
 public static bool GetWindowRect(IntPtr window,out Rect rect) {
  var previous=SetThreadDpiAwarenessContext(new IntPtr(-4));
  try{return NativeWindowRect(window,out rect);}
  finally{if(previous!=IntPtr.Zero)SetThreadDpiAwarenessContext(previous);}
 }
 public struct Rect {public int left,top,right,bottom;}
 public static IntPtr Find(uint process) {
  IntPtr result=IntPtr.Zero;
  EnumWindows((window,data)=>{uint owner;GetWindowThreadProcessId(window,out owner);
   var name=new System.Text.StringBuilder(80);GetClassName(window,name,80);
   if(owner==process && name.ToString()==PetClass){result=window;return false;}return true;},IntPtr.Zero);
  return result;
 }
}
'@
[BattleNativeTest]::PetClass=if($Renderer -eq 'three'){'DesktopPetThree'}else{'RaylibDesktopPet'}
$output=Join-Path $PWD $(if($Renderer -eq 'three'){if($Smooth){'artifacts/three-smooth-mode'}else{'artifacts/three-battle-mode'}}else{'artifacts/battle-mode'})
New-Item -ItemType Directory -Path $output -Force | Out-Null
$log=Join-Path $output 'native.log'
$executable=if($Renderer -eq 'three'){if($Smooth){'build/three-smooth/desktop_pet_three_smooth.exe'}else{'build/three/desktop_pet_three.exe'}}else{'build/desktop_pet.exe'}
$arguments=if($Renderer -eq 'three'){@('--model',$Model,'--mode','battle','--duration','20')}else{@('--model',$Model,'--behavior',$Behavior,'--mode','battle','--frames','1500')}
$pet=Start-Process -FilePath (Resolve-Path $executable).Path -ArgumentList $arguments -WindowStyle Hidden -RedirectStandardOutput $log -RedirectStandardError (Join-Path $output 'native.err') -PassThru
# Keep a process handle so Windows PowerShell can reliably retrieve ExitCode.
$null=$pet.Handle
function Wait-Log([string]$pattern,[int]$seconds=12) {
 $deadline=[DateTime]::UtcNow.AddSeconds($seconds)
 do {
  if($pet.HasExited){throw "Pet exited early: $($pet.ExitCode)"}
  if((Get-Content -LiteralPath $log -Raw) -match $pattern){return}
  Start-Sleep -Milliseconds 80
 }while([DateTime]::UtcNow -lt $deadline)
 throw "Timed out: $pattern"
}
$dummy=[IntPtr]::Zero
try {
 Wait-Log 'MODE battle; battle supported=1'
 if($Renderer -eq 'three'){Wait-Log 'READY Azusa:'}
 $window=[BattleNativeTest]::Find($pet.Id)
 if($window -eq [IntPtr]::Zero){throw 'Missing pet window'}
 if($Renderer -eq 'three'){
  Start-Sleep -Milliseconds 400
  $normalRect=New-Object BattleNativeTest+Rect
  [BattleNativeTest]::GetWindowRect($window,[ref]$normalRect) | Out-Null
  # Scroll only the owned test pet through window messages; never move the
  # user's cursor or inject global mouse input.
  [BattleNativeTest]::PostMessage($window,0x020a,[IntPtr](960*65536),[IntPtr]0) | Out-Null
  Start-Sleep -Milliseconds 700
  $zoomRect=New-Object BattleNativeTest+Rect
  [BattleNativeTest]::GetWindowRect($window,[ref]$zoomRect) | Out-Null
  if(($zoomRect.bottom-$zoomRect.top) -le ($normalRect.bottom-$normalRect.top) -or ($zoomRect.right-$zoomRect.left) -le ($normalRect.right-$normalRect.left)){throw 'Native viewport did not grow to contain the enlarged animation envelope'}
  Start-Sleep -Milliseconds 400
  $steadyRect=New-Object BattleNativeTest+Rect
  [BattleNativeTest]::GetWindowRect($window,[ref]$steadyRect) | Out-Null
  if(($steadyRect.bottom-$steadyRect.top) -ne ($zoomRect.bottom-$zoomRect.top) -or ($steadyRect.right-$steadyRect.left) -ne ($zoomRect.right-$zoomRect.left)){throw 'Animation changed the fixed viewport dimensions'}
 }
 # Non-activating temporary window owned by this test, never a user window.
 $dummy=[BattleNativeTest]::CreateWindowEx(0x08000000,'STATIC','Battle state test',0x10cf0000,20,20,240,90,[IntPtr]::Zero,[IntPtr]::Zero,[IntPtr]::Zero,[IntPtr]::Zero)
 if($dummy -eq [IntPtr]::Zero){throw 'Could not create test window'}
 Start-Sleep -Milliseconds 250
 [BattleNativeTest]::DestroyWindow($dummy) | Out-Null
 $dummy=[IntPtr]::Zero
 Wait-Log 'ACTION play: Victory_Start'
 Wait-Log 'ACTION play: Victory_End'
 if($Renderer -eq 'three'){Wait-Log 'VICTORY facing: render=0'}
 Start-Sleep -Milliseconds 4500
 $text=Get-Content -LiteralPath $log -Raw
 if(([regex]::Matches($text,'ACTION play: Victory_End')).Count -ne 3){throw 'Victory_End did not run exactly three times'}
 foreach($click in 1..3) {
  [BattleNativeTest]::PostMessage($window,0x0201,[IntPtr]1,[IntPtr]0) | Out-Null
  [BattleNativeTest]::PostMessage($window,0x0202,[IntPtr]0,[IntPtr]0) | Out-Null
  Start-Sleep -Milliseconds 120
 }
 Wait-Log 'ACTION play: Vital_Death'
 Wait-Log 'ACTION play: Vital_Retreat'
 $startRect=New-Object BattleNativeTest+Rect
 [BattleNativeTest]::GetWindowRect($window,[ref]$startRect) | Out-Null
 Start-Sleep -Milliseconds 550
 $endRect=New-Object BattleNativeTest+Rect
 [BattleNativeTest]::GetWindowRect($window,[ref]$endRect) | Out-Null
 if($endRect.top -ge $startRect.top){throw 'Retreat did not ascend'}
 Wait-Log 'retirement complete: cafe mode'
 Wait-Log 'MODE cafe; battle supported=1'
 [BattleNativeTest]::PostMessage($window,0x0010,[IntPtr]0,[IntPtr]0) | Out-Null
 if(!$pet.WaitForExit(5000) -or $pet.ExitCode -ne 0){throw "Pet did not exit cleanly: exit=$($pet.ExitCode)"}
 Write-Output "PASS ($Renderer): real combat clips, WinEvent close -> victory x3, own-window triple click -> death/ascending retreat/cafe; log=$log"
}finally {
 if($dummy -ne [IntPtr]::Zero){[BattleNativeTest]::DestroyWindow($dummy) | Out-Null}
 if(!$pet.HasExited) {
  $window=[BattleNativeTest]::Find($pet.Id)
  if($window -ne [IntPtr]::Zero){[BattleNativeTest]::PostMessage($window,0x0010,[IntPtr]0,[IntPtr]0) | Out-Null}
  if(!$pet.WaitForExit(5000)){
   $owned=Get-Process -Id $pet.Id -ErrorAction SilentlyContinue
   if($owned -and $owned.Path -eq (Resolve-Path $executable).Path){Stop-Process -InputObject $owned}
  }
 }
}
