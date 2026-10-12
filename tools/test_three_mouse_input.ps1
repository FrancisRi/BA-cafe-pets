param([string]$Python='C:/Users/leemo/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Set-Location $root
$fixture=Join-Path $root 'artifacts/three-input-check'
New-Item -ItemType Directory -Force -Path $fixture | Out-Null
Copy-Item -LiteralPath build/three/desktop_pet_three.exe,build/three/WebView2Loader.dll -Destination $fixture -Force
Copy-Item -LiteralPath build/three/app,build/three/behavior -Destination $fixture -Recurse -Force
& $Python (Join-Path $PSScriptRoot 'test_three_mouse_input.py')
if($LASTEXITCODE -ne 0){throw 'Actual mouse input test failed'}
