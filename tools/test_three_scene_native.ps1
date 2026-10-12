param([string]$Iori='C:/Users/leemo/Downloads/Iori.glb')
$ErrorActionPreference='Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
$fixture=Join-Path $PWD 'artifacts/three-scene-check'
New-Item -ItemType Directory -Force -Path $fixture,(Join-Path $fixture 'models'),(Join-Path $fixture 'behavior') | Out-Null
Copy-Item -LiteralPath build/three/WebView2Loader.dll -Destination $fixture -Force
Copy-Item -LiteralPath build/three/app -Destination $fixture -Recurse -Force
Copy-Item -LiteralPath design/hibiki-cafe-attention.bt.xml -Destination (Join-Path $fixture 'behavior/template.bt.xml') -Force
$names=@('Azusa','Hibiki','Iori','Azusa-alt','Hibiki-alt','Azusa-extra')
$sources=@('assets/Azusa.glb','assets/Hibiki.glb',$Iori,'assets/Azusa.glb','assets/Hibiki.glb','assets/Azusa.glb')
for($i=0;$i -lt $names.Count;$i++){Copy-Item -LiteralPath $sources[$i] -Destination (Join-Path $fixture ('models/'+$names[$i]+'.glb')) -Force}
@'
[Behavior]
Walking=0
Tracking=0
InputIdleSeconds=600
ContinuedInputSeconds=600
Mode=Cafe
[Three]
Follow=0
OutlineMode=2
MoveInterval=600
SkillProbability=0
'@ | Set-Content -LiteralPath (Join-Path $fixture 'settings.ini')
$prepare=@'
import pathlib,json,sys
p=pathlib.Path(sys.argv[1]);names=['Azusa','Hibiki','Iori','Azusa-alt','Hibiki-alt','Azusa-extra'];rows=[]
for i,n in enumerate(names):
 f=(p/'models'/f'{n}.glb').resolve();h=14695981039346656037
 for c in str(f).lower(): h=((h^ord(c))*1099511628211)&((1<<64)-1)
 rows.append(dict(id=f'{h:x}',path=str(f),name=n,mode='cafe',active=i<5))
(p/'characters.json').write_text(json.dumps(dict(selected=rows[0]['id'],characters=rows)),encoding='utf-8')
'@
& 'C:/Espressif/tools/python/python.exe' -c $prepare $fixture
$test=Start-Process -FilePath (Resolve-Path build/three_scene_native.exe).Path -ArgumentList ('"'+$fixture+'"') -WindowStyle Hidden -RedirectStandardOutput (Join-Path $fixture 'native.log') -RedirectStandardError (Join-Path $fixture 'native.err') -PassThru
$null=$test.Handle
[pscustomobject]@{Id=$test.Id;Log=(Join-Path $fixture 'native.log')}
if(!$test.WaitForExit(60000)){
    $owned=Get-Process -Id $test.Id -ErrorAction SilentlyContinue
    if($owned -and $owned.Path -eq (Resolve-Path build/three_scene_native.exe).Path){$owned | Stop-Process -Force}
    throw 'Scene integration test timed out'
}
$log=Get-Content -LiteralPath (Join-Path $fixture 'native.log') -Raw
$errors=Get-Content -LiteralPath (Join-Path $fixture 'native.err') -Raw
if($test.ExitCode -ne 0 -or $log -notmatch 'PASS: five GLBs' -or $errors -match 'Shader|SCENE TEST|THREE error|THREE loadFailed'){throw "Scene test failed: $errors"}
($log -split "`n" | Select-Object -Last 8) -join "`n"
