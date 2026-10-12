param(
    [Parameter(Mandatory=$true)][string]$Source,
    [string]$Exe = '', [string]$Node = '', [string]$Blender = '',
    [string]$Result = '', [string]$Progress = '', [switch]$Launch
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
function Set-ImportProgress([int]$Percent,[string]$Stage,[string]$Detail='') {
    if (!$Progress) { return }
    try { [IO.File]::WriteAllText($Progress, "$Percent`n$Stage`n$($Detail -replace '[\r\n]',' ')`n", [Text.UTF8Encoding]::new($false)) } catch { Write-Warning $_ }
}
try {
if ($Progress) { $Progress = [IO.Path]::GetFullPath($Progress) }
Set-ImportProgress 0 'prepare'
if (!$Exe) { $Exe = Join-Path $root 'build/desktop_pet.exe' }
$Exe = (Resolve-Path -LiteralPath $Exe).Path
$Source = (Resolve-Path -LiteralPath $Source).Path
# Prefer portable tools shipped with the application; installed tools are a dev fallback.
if (!$Node) {
    $bundled = Join-Path $root 'tools/runtime/node/node.exe'
    if (Test-Path -LiteralPath $bundled) { $Node = $bundled }
    elseif (Get-Command node -ErrorAction SilentlyContinue) { $Node = (Get-Command node).Source }
    else { throw 'Node runtime missing. Supply -Node or bundle tools/runtime/node/node.exe.' }
}
if (!$Blender) {
    $bundled = Join-Path $root 'tools/runtime/blender/blender.exe'
    $installed = 'C:/Program Files/Blender Foundation/Blender 5.2/blender.exe'
    if (Test-Path -LiteralPath $bundled) { $Blender = $bundled }
    elseif (Test-Path -LiteralPath $installed) { $Blender = $installed }
    else { throw 'Blender runtime missing. Supply -Blender or bundle tools/runtime/blender/.' }
}
if (!(Test-Path (Join-Path $root 'artifacts/three-reference/package/build/three.module.js'))) {
    throw 'Pinned Three.js runtime missing. Run tools/setup_wiki_reference.ps1 during preparation.'
}
Write-Output "Node runtime: $Node"
Write-Output "Blender runtime: $Blender"
if (!$Result) { $Result = Join-Path $root 'artifacts/import-result.json' }
$Result = [IO.Path]::GetFullPath($Result)
& $Node (Join-Path $PSScriptRoot 'import_model.mjs') --source $Source --exe $Exe --blender $Blender --result $Result --progress $Progress
if ($LASTEXITCODE -ne 0) { throw "Import failed. See $Result and the conversion logs. Existing pet is unchanged." }
$report = Get-Content -LiteralPath $Result -Raw | ConvertFrom-Json
if ($report.status -ne 'ready_for_visual_review') { throw 'Import did not produce a validated runtime model.' }
Write-Output "Model: $($report.model)"
Write-Output "Behavior: $($report.behavior)"
Write-Output "Clips: $($report.clips.Count); bones: $($report.bones); morph meshes: $($report.morphMeshes)"
if ($Launch) {
    Set-ImportProgress 99 'launching'
    $log = Join-Path (Split-Path $report.model -Parent) 'preview.log'
    $pet = Start-Process -FilePath $Exe -ArgumentList "--model `"$($report.model)`" --behavior `"$($report.behavior)`"" `
        -WindowStyle Hidden -PassThru -RedirectStandardOutput $log -RedirectStandardError "$log.err"
    Start-Sleep -Milliseconds 1500
    if ($pet.HasExited) { throw "Preview exited unexpectedly. See $log" }
    $active = Join-Path (Split-Path $Exe -Parent) 'active-model.ini'
    [IO.File]::WriteAllLines($active, @('[Model]', "Path=$($report.model)", "Behavior=$($report.behavior)"), [Text.Encoding]::Unicode)
    Write-Output "Preview PID: $($pet.Id)"
}

Set-ImportProgress 100 'complete'
} catch {
    $alreadyFailed = $false
    if ($Progress -and (Test-Path -LiteralPath $Progress)) {
        $lines = [IO.File]::ReadAllLines($Progress)
        $alreadyFailed = $lines.Length -ge 2 -and $lines[1] -eq 'failed'
    }
    if (!$alreadyFailed) { Set-ImportProgress 0 'failed' $_.Exception.Message }
    throw
}
