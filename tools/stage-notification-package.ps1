param(
    [Parameter(Mandatory)][string]$Executable,
    [Parameter(Mandatory)][string]$Destination
)

$ErrorActionPreference = 'Stop'
$exe = Get-Item -LiteralPath $Executable
$bin = $exe.Directory.FullName
$root = Split-Path -Parent $PSScriptRoot
$out = [IO.Path]::GetFullPath($Destination)
if (Test-Path -LiteralPath $out) { throw 'Use a new package staging directory.' }
$assets = Join-Path $out VisualAssets
New-Item -ItemType Directory -Path $assets -Force | Out-Null
Copy-Item -LiteralPath $exe.FullName -Destination (Join-Path $out 'BA-Cafe-Pets.exe')
Copy-Item -LiteralPath (Join-Path $root 'packaging/notifications/AppxManifest.xml') -Destination $out
[xml]$manifest = Get-Content -LiteralPath (Join-Path $out 'AppxManifest.xml')
$version = $exe.VersionInfo.ProductVersion
if ($version -notmatch '^\d+\.\d+\.\d+$') { throw "Unexpected application version $version" }
$manifest.Package.Identity.Version = "$version.0"
$manifest.Save((Join-Path $out 'AppxManifest.xml'))
foreach ($file in @('defaults.ini','model/Shiroko.glb','behavior/template.bt.xml',
    'behavior/battle-mode.drawio','behavior/battle-mode.mmd','LICENSE.md','NOTICE.md','CREDITS.md',
    'docs/LICENSE-SCOPE.md','docs/DEVELOPMENT.md')) {
    $target = Join-Path $out $file
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
    Copy-Item -LiteralPath (Join-Path $bin $file) -Destination $target
}
Copy-Item -LiteralPath (Join-Path $bin 'licenses') -Destination $out -Recurse
Add-Type -AssemblyName System.Drawing
$icon = [Drawing.Icon]::ExtractAssociatedIcon($exe.FullName)
try {
    $source = $icon.ToBitmap()
    try {
        foreach ($entry in @{ 'StoreLogo'=50; 'Square44'=44; 'Square150'=150 }.GetEnumerator()) {
            $image = [Drawing.Bitmap]::new($source, $entry.Value, $entry.Value)
            try { $image.Save((Join-Path $assets ($entry.Key + '.png')), [Drawing.Imaging.ImageFormat]::Png) }
            finally { $image.Dispose() }
        }
    } finally { $source.Dispose() }
} finally { $icon.Dispose() }
Write-Output (Join-Path $out 'AppxManifest.xml')
