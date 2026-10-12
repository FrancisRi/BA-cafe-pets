param([string]$BuildDirectory = 'build', [string]$Tree = '')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root "$BuildDirectory\desktop_pet.exe"
if (!$Tree) {
    $active = Join-Path (Split-Path $exe -Parent) 'active-model.ini'
    if (Test-Path -LiteralPath $active) {
        $entry = Get-Content -LiteralPath $active | Where-Object { $_.StartsWith('Behavior=') } | Select-Object -First 1
        if ($entry) { $Tree = $entry.Substring(9) }
    }
    if (!$Tree) { $Tree = Join-Path $root 'design\hibiki-cafe-attention.bt.xml' }
}
$Tree = (Resolve-Path -LiteralPath $Tree).Path
$output = Join-Path $root 'artifacts\behavior'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$trace = Join-Path $output "$stamp.btlog"
$petProcess = Start-Process -FilePath $exe -WorkingDirectory $root -WindowStyle Hidden -PassThru `
    -ArgumentList "--behavior `"$Tree`" --bt-log `"$trace`"" `
    -RedirectStandardOutput (Join-Path $output "$stamp.log") `
    -RedirectStandardError (Join-Path $output "$stamp.err")
Write-Output "Pet PID: $($petProcess.Id)"
Write-Output "Edit in Groot2 and save: $Tree"
Write-Output "Execution trace: $trace"
Write-Output 'Middle-click opens settings. Close the pet with Ctrl+Alt+Q before starting another copy.'
