param([string]$BuildDirectory = 'build', [string]$Node = 'node')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root "$BuildDirectory\desktop_pet.exe"
$output = Join-Path $root 'artifacts\wiki-poses'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$clips = [ordered]@{Cafe_Idle=4.8; Cafe_Reaction=5.667; Cafe_Walk=1.267; Formation_Idle=4.0; Formation_Pickup=12.867; Exs=4.0; Tactical_Start=2.0}
$rows = @()
foreach ($clip in $clips.Keys) {
    foreach ($fraction in @(0, 0.25, 0.5, 0.9)) {
        # Compare exact 60 Hz sample instants, excluding the cyclic endpoint.
        $time = [math]::Floor($clips[$clip]*$fraction*60)/60
        $seconds = $time.ToString('F8', [Globalization.CultureInfo]::InvariantCulture)
        $stem = Join-Path $output "$clip-$fraction"
        $pet = Start-Process -FilePath $exe -WorkingDirectory $root -WindowStyle Hidden -PassThru `
            -ArgumentList "--clip $clip --sample $seconds --frames 1 --pose-dump `"$stem.bin`" --snapshot `"$stem.png`"" `
            -RedirectStandardOutput "$stem.log" -RedirectStandardError "$stem.err"
        $pet.WaitForExit(); $pet.Refresh()
        if ($pet.ExitCode -ne 0) { throw "Pose dump failed: $clip at $seconds" }
        & $Node (Join-Path $PSScriptRoot 'compare_wiki_pose.mjs') $clip $seconds "$stem.bin" > "$stem.json"
        if ($LASTEXITCODE -ne 0) { throw "Three.js reference failed: $clip at $seconds" }
        $comparison = Get-Content -LiteralPath "$stem.json" -Raw | ConvertFrom-Json
        foreach ($material in $comparison.results) {
            $rows += [pscustomobject]@{Clip=$clip; Seconds=$seconds; Material=$material.material; ConversionMax=$material.conversion.max; RaylibMax=$material.raylib.max}
        }
    }
    Write-Output "Compared $clip at 4 poses against Wiki Three.js"
}
$rows | Export-Csv -LiteralPath (Join-Path $output 'summary.csv') -NoTypeInformation -Encoding utf8
$conversionMax = ($rows.ConversionMax | Measure-Object -Maximum).Maximum
Write-Output "Maximum source-to-converted geometry distance: $conversionMax model units"
# Small tolerance for 60 Hz sampling/export precision, not a visual acceptance.
# Keep renderer differences visible, including raylib TRS/shear and unskinned
# animated attachments. Do not misreport these as a complete visual pass.
$rendererDifferences = $rows | Where-Object RaylibMax -gt 0.0005
if ($rendererDifferences) {
    $rendererDifferences | Group-Object Material | ForEach-Object {
        $maximum = ($_.Group.RaylibMax | Measure-Object -Maximum).Maximum
        Write-Output "RENDERER LIMIT: $($_.Name), maximum $maximum model units"
    }
}
if ($conversionMax -gt 0.0005) { throw 'Converted geometry exceeds strict 0.0005 tolerance; inspect summary.csv. Renderer differences are reported above.' }
Write-Output 'PASS: exported geometry regression. Runtime differences are reported separately; user visual review still required.'
