param([int]$Jobs=4,[switch]$SkipBuild,[string]$RootExecutablePath)
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$project=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'CMakeLists.txt') -Raw
if($project -notmatch 'project\(ba_cafe_lite_native VERSION (\d+\.\d+\.\d+)'){throw 'Cannot read native release version'}
$version=$Matches[1]
$base=Join-Path $PSScriptRoot 'build/bin'
$dist=Join-Path $root 'dist'
$output=Join-Path $dist ('BA-Cafe-Pets-'+$version+'-x64.exe')
$zip=Join-Path $dist ('BA-Cafe-Pets-'+$version+'-portable-x64.zip')
if((Test-Path -LiteralPath $output) -or (Test-Path -LiteralPath $zip)){throw 'Release already exists; increment the native CMake project version first'}
if(!$SkipBuild){& (Join-Path $PSScriptRoot 'build.ps1') -Jobs $Jobs -ExecutableName ba_cafe_lite_native_release -Release}
$core=Join-Path $base 'ba_cafe_lite_native_release.exe'
$launcher=Join-Path $base 'ba_cafe_lite_release_launcher.exe'
foreach($binary in @($core,$launcher)){
    $info=(Get-Item -LiteralPath $binary).VersionInfo
    if($info.ProductName -ne 'BA Cafe Pets' -or $info.ProductVersion -ne $version){throw 'Release binaries do not match the current source version'}
}
$work=Join-Path $root ('artifacts/native-release-'+$version+'/'+[guid]::NewGuid().ToString('N'))
$stage=Join-Path $work 'stage'
New-Item -ItemType Directory -Force -Path $stage,$dist | Out-Null
Copy-Item -LiteralPath $core -Destination (Join-Path $stage 'ba_cafe_lite.exe')
$allowed=@('models/Shiroko.glb','defaults.ini','behavior/template.bt.xml','behavior/battle-mode.drawio','behavior/battle-mode.mmd',
    'README.txt','README.md','CHANGELOG.md','CREDITS.md','NOTICE.md','licenses/NOTICE.txt','licenses/raylib.txt','licenses/GLFW.txt','licenses/cgltf.txt',
    'licenses/BehaviorTree.CPP.txt','licenses/tinyxml2.txt','licenses/minitrace.txt','licenses/minicoro.txt','licenses/lexy.txt',
    'licenses/GCC-GPL3.txt','licenses/GCC-runtime-exception.txt')
foreach($relative in $allowed){
    $destination=Join-Path $stage $relative
    New-Item -ItemType Directory -Force -Path (Split-Path $destination -Parent) | Out-Null
    Copy-Item -LiteralPath (Join-Path $base $relative) -Destination $destination
}
$stub=Join-Path $work 'launcher.exe'
Copy-Item -LiteralPath $launcher -Destination $stub
# Strip distribution copies only; leave developer binaries and user data intact.
$strip=(Get-Command strip -ErrorAction SilentlyContinue).Source
if(!$strip){$strip=(Get-ChildItem -Path "$env:LOCALAPPDATA/Microsoft/WinGet/Packages/BrechtSanders.WinLibs.*/mingw64/bin/strip.exe" -ErrorAction SilentlyContinue | Select-Object -First 1).FullName}
if($strip){
    foreach($binary in @((Join-Path $stage 'ba_cafe_lite.exe'),$stub)){
        & $strip --strip-unneeded $binary
        if($LASTEXITCODE -ne 0){throw 'Cannot strip distribution executable'}
    }
}
$files=Get-ChildItem -LiteralPath $stage -File -Recurse | Sort-Object FullName
$manifest=@{project='BA-cafe-pets';version=$version;architecture='x64';engine='raylib-cgltf';defaultModel='Shiroko';upstream='https://github.com/maxiumlee/ba-cafe-lite';files=@()}
foreach($file in $files){
    $manifest.files+=@{path=[IO.Path]::GetRelativePath($stage,$file.FullName).Replace('\','/');size=$file.Length;sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash}
}
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $stage 'release.json') -Encoding utf8
$files=Get-ChildItem -LiteralPath $stage -File -Recurse | Sort-Object FullName
$ddf=Join-Path $work 'bundle.ddf'
$lines=@('.OPTION EXPLICIT','.Set Cabinet=ON','.Set Compress=ON','.Set CompressionType=LZX','.Set CompressionMemory=21',
    '.Set MaxDiskSize=0','.Set MaxCabinetSize=0','.Set CabinetNameTemplate=bundle.cab',
    ('.Set DiskDirectoryTemplate="'+$work+'"'),('.Set RptFileName="'+(Join-Path $work 'bundle.rpt')+'"'),('.Set InfFileName="'+(Join-Path $work 'bundle.inf')+'"'))
foreach($file in $files){$lines+=('"'+$file.FullName+'" "'+[IO.Path]::GetRelativePath($stage,$file.FullName)+'"')}
$lines | Set-Content -LiteralPath $ddf -Encoding ascii
& (Join-Path $env:SystemRoot 'System32/makecab.exe') /v0 /f $ddf *> (Join-Path $work 'makecab.log')
if($LASTEXITCODE -ne 0){throw 'CAB compression failed; see makecab.log in release artifacts'}
$cab=Join-Path $work 'bundle.cab'
$archiveSize=(Get-Item -LiteralPath $cab).Length
$expanded=($files | Measure-Object Length -Sum).Sum
$archiveHash=(Get-FileHash -LiteralPath $cab -Algorithm SHA256).Hash
$footer=New-Object IO.MemoryStream
$writer=New-Object IO.BinaryWriter($footer)
$writer.Write([Text.Encoding]::ASCII.GetBytes("BAPET01`0"))
$writer.Write([uint64](Get-Item -LiteralPath $stub).Length)
$writer.Write([uint64]$archiveSize)
$writer.Write([uint64]$expanded)
$hashBytes=for($i=0;$i -lt 64;$i+=2){[Convert]::ToByte($archiveHash.Substring($i,2),16)}
$writer.Write([byte[]]$hashBytes);$writer.Flush()
if($footer.Length -ne 64){throw 'Invalid launcher footer size'}
$out=[IO.File]::Create($output)
try{
    foreach($file in @($stub,$cab)){
        $input=[IO.File]::OpenRead($file)
        try{$input.CopyTo($out)}finally{$input.Dispose()}
    }
    $out.Write($footer.ToArray(),0,64)
}finally{$out.Dispose();$writer.Dispose();$footer.Dispose()}
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::CreateFromDirectory($stage,$zip,[IO.Compression.CompressionLevel]::Optimal,$false)
foreach($file in @($output,$zip)){
    $hash=(Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash
    ($hash+'  '+[IO.Path]::GetFileName($file)) | Set-Content -LiteralPath ($file+'.sha256') -Encoding ascii
}
Copy-Item -LiteralPath (Join-Path $stage 'README.txt') -Destination (Join-Path $dist ('BA-Cafe-Pets-'+$version+'-README.txt'))
Copy-Item -LiteralPath (Join-Path $stage 'release.json') -Destination (Join-Path $dist ('BA-Cafe-Pets-'+$version+'-release.json'))
$entry=if($RootExecutablePath){[IO.Path]::GetFullPath($RootExecutablePath)}else{Join-Path $root 'BA-Cafe-Pets.exe'}
Copy-Item -LiteralPath $output -Destination $entry -Force
[pscustomobject]@{EXE=$output;RootEXE=$entry;EXEMiB=[math]::Round((Get-Item $output).Length/1MB,2);ZIP=$zip;ZIPMiB=[math]::Round((Get-Item $zip).Length/1MB,2);ExpandedMiB=[math]::Round($expanded/1MB,2);Artifacts=$work}
