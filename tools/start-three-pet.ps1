param([string]$Source='', [ValidateSet('','cafe','battle')][string]$Mode='', [switch]$Smooth)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$exe=Join-Path $root $(if($Smooth){'build/three-smooth/desktop_pet_three_smooth.exe'}else{'build/three/desktop_pet_three.exe'})
if(!(Test-Path -LiteralPath $exe)) {throw 'Build desktop_pet_three first. See experimental/three/README.md.'}
if($Smooth){
    foreach($name in @('settings.ini','active-model.ini')){
        $target=Join-Path (Split-Path $exe) $name
        $original=Join-Path (Join-Path $root 'build/three') $name
        if(!(Test-Path -LiteralPath $target) -and (Test-Path -LiteralPath $original)){Copy-Item -LiteralPath $original -Destination $target}
    }
    Get-ChildItem -LiteralPath (Join-Path $root 'build/three/behavior') -Filter '*.bt.xml' -File | ForEach-Object {
        $target=Join-Path (Join-Path (Split-Path $exe) 'behavior') $_.Name
        if(!(Test-Path -LiteralPath $target)){Copy-Item -LiteralPath $_.FullName -Destination $target}
    }
}
$logPrefix=if($Smooth){'three-smooth-live'}else{'three-live'}
$options=@{FilePath=$exe;WorkingDirectory=$root;WindowStyle='Hidden';PassThru=$true;
    RedirectStandardOutput=(Join-Path $root "artifacts/$logPrefix.log");RedirectStandardError=(Join-Path $root "artifacts/$logPrefix.err")}
if($Source) {
    $sourcePath=(Resolve-Path -LiteralPath $Source).Path
    $options.ArgumentList="--model `"$sourcePath`""
}
if($Mode) { $options.ArgumentList=([string]$options.ArgumentList+" --mode $Mode").Trim() }
$pet=Start-Process @options
Write-Output "Three.js pet PID: $($pet.Id)"
Write-Output 'Middle-click for shared settings. Ctrl+Alt+Shift+Q closes the selected character; the last one asks before exit.'
