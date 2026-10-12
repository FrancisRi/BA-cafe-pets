param([string]$Version='0.1.2')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Set-Location $root
$release=(Resolve-Path ('dist/ba_cafe_lite-'+$Version+'-x64.exe')).Path
$testUserRoot=Join-Path $root ('artifacts/release-final-test-'+$Version+' 用户')
if(Test-Path -LiteralPath $testUserRoot){throw 'Fresh release test directory already exists'}
New-Item -ItemType Directory -Path $testUserRoot | Out-Null
$env:BA_PET_HOME=$testUserRoot
function Run-Release([string[]]$Arguments){
    $watch=[Diagnostics.Stopwatch]::StartNew()
    $probe=Start-Process -FilePath $release -ArgumentList $Arguments -WindowStyle Hidden -PassThru
    $null=$probe.Handle
    if(!$probe.WaitForExit(45000)){throw ('Release test timed out: PID='+$probe.Id)}
    if($probe.ExitCode -ne 0){throw ('Release returned '+$probe.ExitCode)}
    [math]::Round($watch.Elapsed.TotalSeconds,2)
}
$cold=Run-Release @('--exit-after','5')
$cache=Get-ChildItem -LiteralPath (Join-Path $testUserRoot 'app') -Directory | Select-Object -First 1
$manifest=Get-Content -LiteralPath (Join-Path $cache.FullName 'release.json') -Raw | ConvertFrom-Json
if($manifest.project -ne 'ba_cafe_lite' -or (Get-Item $release).VersionInfo.ProductName -ne 'ba_cafe_lite'){throw 'Release branding must match the project name'}
foreach($entry in @($manifest.files)+@($manifest.runtimeFiles)){
    $file=Join-Path $cache.FullName $entry.path
    if(!(Test-Path -LiteralPath $file) -or (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $entry.sha256){throw ('Extracted file differs from original: '+$entry.path)}
}
$settings=Join-Path $testUserRoot 'data/settings.ini'
if((Get-FileHash -LiteralPath $settings).Hash -ne (Get-FileHash -LiteralPath artifacts/release-defaults.ini).Hash){throw 'First-launch defaults must exactly match the captured user settings'}
$log=Join-Path $testUserRoot 'data/logs/pet.log'
$errors=Join-Path $testUserRoot 'data/logs/pet.err'
$text=Get-Content -LiteralPath $log -Raw
if($text -notmatch 'WEBVIEW runtime: bundled fixed version' -or $text -notmatch 'READY Shiroko:'){throw 'Default Shiroko must render using the bundled runtime'}
$expectedMode=if((Get-Content -LiteralPath $settings -Raw) -match 'Mode=Battle'){'battle'}else{'cafe'}
if($text -notmatch ('MODE '+$expectedMode+';')){throw 'First-launch mode must match defaults'}
$history=Get-Content -LiteralPath (Join-Path $testUserRoot 'data/characters.json') -Raw | ConvertFrom-Json
if($history.characters.Count -ne 1 -or $history.characters[0].name -ne 'Shiroko'){throw 'Fresh release must start with only Shiroko'}
$marker=Get-Item -LiteralPath (Join-Path $cache.FullName '.ready')
$modified=(Get-Content -LiteralPath $settings -Raw).Replace('InputIdleSeconds=10','InputIdleSeconds=13')
[IO.File]::WriteAllText($settings,$modified,[Text.UTF8Encoding]::new($false))
$customHash=(Get-FileHash -LiteralPath $settings).Hash
$warm=Run-Release @('--exit-after','3')
if((Get-FileHash -LiteralPath $settings).Hash -ne $customHash){throw 'Subsequent launches must preserve user-edited settings'}
if((Get-Item -LiteralPath $marker.FullName).LastWriteTimeUtc -ne $marker.LastWriteTimeUtc){throw 'Warm launches must reuse cached runtime'}
$model=(Resolve-Path 'C:/Users/leemo/Downloads/Iori.glb').Path
$sourceHash=(Get-FileHash -LiteralPath $model).Hash
$import=Run-Release @('--model',('"'+$model+'"'),'--exit-after','3')
if((Get-Content -LiteralPath $log -Raw) -notmatch 'READY Iori:'){throw 'Direct original GLB import must succeed'}
if((Get-FileHash -LiteralPath $model).Hash -ne $sourceHash){throw 'Import must not modify the source GLB'}
if((Get-Content -LiteralPath $errors -Raw) -match 'Shader|COM callback|Startup:|Message:|THREE error|loadFailed'){throw 'Release has renderer/host errors'}
[pscustomobject]@{Result='PASS';ColdSeconds=$cold;WarmSeconds=$warm;GLBSeconds=$import;FilesChecked=$manifest.files.Count+$manifest.runtimeFiles.Count;DefaultModel='Shiroko';DefaultMode=$expectedMode;PersistentSettings=$true;Runtime='bundled';Home=$testUserRoot}
