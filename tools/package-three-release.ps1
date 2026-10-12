param([string]$Version='0.1.2', [string]$Settings='artifacts/release-defaults.ini',
      [string]$Runtime='tools/runtime/webview2/Microsoft.WebView2.FixedVersionRuntime.155.0.4283.45.x64',
      [string]$RuntimeCab='tools/downloads/WebView2-155.0.4283.45-x64.cab',[switch]$Resume)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Set-Location $root
if($Version -notmatch '^\d+\.\d+\.\d+([a-z0-9.-]+)?$'){throw 'Invalid release version'}
foreach($binary in @('build/three/desktop_pet_three.exe','build/three_release_launcher.exe')){
    $info=(Get-Item -LiteralPath $binary).VersionInfo
    if($info.ProductName -ne 'ba_cafe_lite' -or $info.ProductVersion -ne $Version){throw 'Build ba_cafe_lite with the matching CMake project version before packaging'}
}
$output=Join-Path $root ('dist/ba_cafe_lite-'+$Version+'-x64.exe')
$work=Join-Path $root ('artifacts/three-release-'+$Version)
$stage=Join-Path $work 'stage'
if(Test-Path -LiteralPath $output){throw 'Release EXE already exists; choose a new version'}
if((Test-Path -LiteralPath $stage) -and !$Resume){throw 'Release stage already exists; choose a new version or resume staging'}
$runtimePath=(Resolve-Path $Runtime).Path
$signature=Get-AuthenticodeSignature (Join-Path $runtimePath 'msedgewebview2.exe')
if($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'Microsoft Corporation'){throw 'Runtime must have a valid Microsoft signature'}
New-Item -ItemType Directory -Force -Path $stage,(Join-Path $root 'dist') | Out-Null
Copy-Item -LiteralPath build/three/desktop_pet_three.exe -Destination (Join-Path $stage 'ba_cafe_lite.exe')
Copy-Item -LiteralPath build/three/WebView2Loader.dll -Destination $stage
Copy-Item -LiteralPath build/three/app,build/three/licenses -Destination $stage -Recurse -Force
New-Item -ItemType Directory -Force -Path (Join-Path $stage 'models'),(Join-Path $stage 'behavior') | Out-Null
Copy-Item -LiteralPath assets/Shiroko.glb -Destination (Join-Path $stage 'models')
Copy-Item -LiteralPath design/hibiki-cafe-attention.bt.xml -Destination (Join-Path $stage 'behavior/template.bt.xml')
Copy-Item -LiteralPath experimental/three/design/battle-mode.drawio,experimental/three/design/battle-mode.mmd -Destination (Join-Path $stage 'behavior')
Copy-Item -LiteralPath $Settings -Destination (Join-Path $stage 'defaults.ini')
if((Get-FileHash -LiteralPath $RuntimeCab -Algorithm SHA256).Hash -ne 'EC533BF1B7354D288703CF821A2D640D0130108224F93E330BB9C09FAF3F8010'){throw 'Original Microsoft runtime CAB checksum mismatch'}
Copy-Item -LiteralPath $RuntimeCab -Destination (Join-Path $stage 'runtime.cab')
Copy-Item -LiteralPath artifacts/release-licenses/GCC-GPL3.txt,artifacts/release-licenses/GCC-runtime-exception.txt -Destination (Join-Path $stage 'licenses')
Copy-Item -LiteralPath vendor/raylib-6.0/src/external/glfw/LICENSE.md -Destination (Join-Path $stage 'licenses/GLFW.txt')
foreach($part in @('tinyxml2','minitrace','minicoro','lexy')){
    $notice=Get-ChildItem -LiteralPath (Join-Path $root ('vendor/BehaviorTree.CPP-4.8.0/3rdparty/'+$part)) -Filter 'LICENSE*' | Select-Object -First 1
    Copy-Item -LiteralPath $notice.FullName -Destination (Join-Path $stage ('licenses/'+$part+'.txt'))
}
Copy-Item -LiteralPath experimental/three/RELEASE_README.txt -Destination (Join-Path $stage 'README.txt')
@'
Three.js: https://github.com/mrdoob/three (MIT; app/three/LICENSE)
BehaviorTree.CPP: https://github.com/BehaviorTree/BehaviorTree.CPP (MIT)
raylib: https://github.com/raysan5/raylib (zlib)
WebView2 SDK and Fixed Runtime: Microsoft Corporation.
The original Fixed Runtime files and embedded third-party credits are retained.
Run runtime/webview2/show_third_party_software_licenses.bat to view their notices.
GCC libstdc++/libgcc: GPLv3 with GCC Runtime Library Exception.
Shiroko.glb: original user-provided BlueArchiveModels archive, unchanged.
Model collection: https://github.com/lihaohong6/BlueArchiveModels
Viewer reference: https://bluearchive.wiki/wiki/Models
Blue Archive character/model rights belong to their respective owners.
'@ | Set-Content -LiteralPath (Join-Path $stage 'licenses/NOTICE.txt') -Encoding utf8
# A release contains neither developer/user profiles nor personal history/paths.
foreach($name in @('settings.ini','characters.json','active-model.ini','webview-profile')){
    if(Test-Path -LiteralPath (Join-Path $stage $name)){throw "Unexpected personal state in release: $name"}
}
$files=Get-ChildItem -LiteralPath $stage -File -Recurse | Where-Object {$_.Name -notin @('release.json','runtime.cab')} | Sort-Object FullName
$manifest=@{project='ba_cafe_lite';version=$Version;architecture='x64';defaultModel='Shiroko';runtimeVersion=(Get-Item $signature.Path).VersionInfo.ProductVersion;files=@();runtimeFiles=@()}
foreach($file in $files){$manifest.files+=@{path=[IO.Path]::GetRelativePath($stage,$file.FullName).Replace('\','/');size=$file.Length;sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash}}
foreach($file in (Get-ChildItem -LiteralPath $runtimePath -File -Recurse)){$manifest.runtimeFiles+=@{path='runtime/webview2/'+[IO.Path]::GetRelativePath($runtimePath,$file.FullName).Replace('\','/');size=$file.Length;sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash}}
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $stage 'release.json') -Encoding utf8
$files=Get-ChildItem -LiteralPath $stage -File -Recurse | Sort-Object FullName
$ddf=Join-Path $work 'bundle.ddf'
$lines=@('.OPTION EXPLICIT','.Set Cabinet=ON','.Set Compress=ON','.Set CompressionType=LZX','.Set CompressionMemory=21',
    '.Set MaxDiskSize=0','.Set MaxCabinetSize=0','.Set CabinetNameTemplate=bundle.cab',
    ('.Set DiskDirectoryTemplate="'+$work+'"'),('.Set RptFileName="'+(Join-Path $work 'bundle.rpt')+'"'),('.Set InfFileName="'+(Join-Path $work 'bundle.inf')+'"'))
foreach($file in ($files | Where-Object {$_.Name -ne 'runtime.cab'})){$lines+=('"'+$file.FullName+'" "'+[IO.Path]::GetRelativePath($stage,$file.FullName)+'"')}
$lines+=@('.New Folder','.Set Compress=OFF',('"'+(Join-Path $stage 'runtime.cab')+'" "runtime.cab"'))
$lines | Set-Content -LiteralPath $ddf -Encoding ascii
& (Join-Path $env:SystemRoot 'System32/makecab.exe') /v0 /f $ddf *> (Join-Path $work 'makecab.log')
if($LASTEXITCODE -ne 0){throw 'CAB compression failed'}
$cab=Join-Path $work 'bundle.cab'
$stub=(Resolve-Path build/three_release_launcher.exe).Path
$archiveSize=(Get-Item $cab).Length
$expanded=($files | Measure-Object Length -Sum).Sum+(Get-ChildItem -LiteralPath $runtimePath -File -Recurse | Measure-Object Length -Sum).Sum
$archiveHash=(Get-FileHash -LiteralPath $cab -Algorithm SHA256).Hash
$footer=New-Object IO.MemoryStream
$writer=New-Object IO.BinaryWriter($footer)
$writer.Write([Text.Encoding]::ASCII.GetBytes("BAPET01`0"))
$writer.Write([uint64](Get-Item $stub).Length);$writer.Write([uint64]$archiveSize);$writer.Write([uint64]$expanded)
$hashBytes=for($i=0;$i -lt 64;$i+=2){[Convert]::ToByte($archiveHash.Substring($i,2),16)}
$writer.Write([byte[]]$hashBytes);$writer.Flush()
if($footer.Length -ne 64){throw 'Invalid launcher footer size'}
$out=[IO.File]::Create($output)
try{foreach($file in @($stub,$cab)){$input=[IO.File]::OpenRead($file);try{$input.CopyTo($out)}finally{$input.Dispose()}};$out.Write($footer.ToArray(),0,64)}finally{$out.Dispose();$writer.Dispose();$footer.Dispose()}
$releaseHash=(Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash
($releaseHash+'  '+[IO.Path]::GetFileName($output)) | Set-Content -LiteralPath ($output+'.sha256') -Encoding ascii
[pscustomobject]@{File=$output;MiB=[math]::Round((Get-Item $output).Length/1MB,1);UnpackedMiB=[math]::Round($expanded/1MB,1);SHA256=$releaseHash}
