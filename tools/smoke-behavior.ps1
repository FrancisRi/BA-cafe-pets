param([string]$BuildDirectory = 'build')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root "$BuildDirectory\desktop_pet.exe"
$output = Join-Path $root ('artifacts\behavior-smoke\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Force -Path $output | Out-Null
$treePath = Join-Path $output 'test.bt.xml'
$log = Join-Path $output 'pet.log'
$xml = (Get-Content -LiteralPath (Join-Path $root 'tests\default.bt.xml') -Raw).
    Replace('min_seconds="5"', 'min_seconds="0.25"').
    Replace('max_seconds="12"', 'max_seconds="0.25"').
    Replace('Cafe_Walk;Formation_Pickup;Exs', 'Cafe_Walk')
[IO.File]::WriteAllText($treePath, $xml)
$petProcess = Start-Process -FilePath $exe -WorkingDirectory $root -WindowStyle Hidden -PassThru `
    -ArgumentList "--behavior `"$treePath`" --bt-log `"$output\trace.btlog`" --check-window --frames 240 --snapshot `"$output\preview.png`"" `
    -RedirectStandardOutput $log -RedirectStandardError (Join-Path $output 'pet.err')
# Cache the process handle before exit (Windows PowerShell 5 otherwise may lose ExitCode).
$null = $petProcess.Handle
function Wait-Log([string]$Needle, [int]$Count = 1) {
    $deadline = [DateTime]::UtcNow.AddSeconds(20)
    while ([DateTime]::UtcNow -lt $deadline) {
        if (Test-Path -LiteralPath $log) {
            $matchesFound = @(Select-String -LiteralPath $log -SimpleMatch $Needle)
            if ($matchesFound.Count -ge $Count) { return }
        }
        if ($petProcess.HasExited) { throw "Pet exited before '$Needle'; inspect $log" }
        Start-Sleep -Milliseconds 100
    }
    throw "Timed out waiting for '$Needle'; inspect $log"
}
try {
    Wait-Log 'ACTION play: Cafe_Walk'
    Wait-Log 'ACTION idle: Cafe_Idle' 2
    [IO.File]::WriteAllText($treePath, $xml.Replace('Cafe_Reaction', 'MissingClip'))
    Wait-Log 'BT rejected; keeping previous tree: Unknown animation: MissingClip'
    [IO.File]::WriteAllText($treePath, '<root BTCPP_format="4"><BehaviorTree ID="Edited"><LoopAnimation clip="Formation_Idle"/></BehaviorTree></root>')
    Wait-Log 'BT loaded revision 2'
    Wait-Log 'ACTION idle: Formation_Idle'
    if (!$petProcess.WaitForExit(20000)) { throw 'Pet did not finish frame-limited smoke test' }
    $petProcess.WaitForExit()
    if ($petProcess.ExitCode -ne 0) { throw "Pet failed: $($petProcess.ExitCode)" }
    if (!(Test-Path -LiteralPath "$output\preview.png")) { throw 'Missing rendered snapshot' }
    if (!(Select-String -LiteralPath $log -SimpleMatch 'transparent pass-through=1 opaque hit=1 topmost=1')) { throw 'Window check did not pass' }
    if (!(Test-Path -LiteralPath "$output\trace-2.btlog")) { throw 'Missing reloaded-tree trace' }
    Write-Output 'PASS: random -> Idle, invalid save rejected, valid save applied, window hit testing, rendering, log segmentation'
    Select-String -LiteralPath $log -SimpleMatch 'Rendered' | ForEach-Object Line
    Write-Output "Artifacts: $output"
} finally {
    if (!$petProcess.HasExited) { Stop-Process -Id $petProcess.Id }
    $petProcess.Dispose()
}
