param([string]$BuildDirectory = 'build')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root "$BuildDirectory\desktop_pet.exe"
$output = Join-Path $root 'artifacts\poses'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$clips = [ordered]@{
    Cafe_Idle = 4.8
    Cafe_Reaction = 5.667
    Cafe_Walk = 1.267
    Formation_Idle = 4.0
    Formation_Pickup = 12.867
    Exs = 4.0
    Tactical_Start = 2.0
}
foreach ($clip in $clips.Keys) {
    $sampleIndex = 0
    foreach ($fraction in @(0.0, 0.25, 0.5, 0.9)) {
        $seconds = ($clips[$clip]*$fraction).ToString('F3', [Globalization.CultureInfo]::InvariantCulture)
        $base = Join-Path $output "$clip-$sampleIndex"
        $petProcess = Start-Process -FilePath $exe -WorkingDirectory $root -WindowStyle Hidden -PassThru `
            -ArgumentList "--clip $clip --sample $seconds --frames 1 --snapshot `"$base.png`"" `
            -RedirectStandardOutput "$base.log" -RedirectStandardError "$base.err"
        $petProcess.WaitForExit()
        $petProcess.Refresh()
        if ($petProcess.ExitCode -ne 0 -or !(Test-Path -LiteralPath "$base.png")) {
            throw "Failed $clip at $seconds seconds; exit $($petProcess.ExitCode)"
        }
        $sampleIndex++
    }
    Write-Output "PASS: $clip (4 sampled poses)"
}
Add-Type -AssemblyName System.Drawing
$sheet = [Drawing.Bitmap]::new(4*300, 7*320)
$graphics = [Drawing.Graphics]::FromImage($sheet)
$graphics.Clear([Drawing.Color]::FromArgb(45, 51, 64))
$font = [Drawing.Font]::new('Segoe UI', 10)
$row = 0
foreach ($clip in $clips.Keys) {
    for ($column = 0; $column -lt 4; $column++) {
        $image = [Drawing.Image]::FromFile((Join-Path $output "$clip-$column.png"))
        $graphics.DrawImage($image, $column*300, $row*320+20, 300, 300)
        $graphics.DrawString("$clip ($column)", $font, [Drawing.Brushes]::White, $column*300+8, $row*320+2)
        $image.Dispose()
    }
    $row++
}
$sheet.Save((Join-Path $root 'artifacts\animation-review.png'), [Drawing.Imaging.ImageFormat]::Png)
$font.Dispose()
$graphics.Dispose()
$sheet.Dispose()
