param([string]$ImportReport = 'artifacts/azusa-import-result.json', [string]$Destination = 'artifacts/portable-pet',
      [string]$BlenderDirectory = 'C:/Program Files/Blender Foundation/Blender 5.2', [string]$Node = '')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$report=Get-Content -LiteralPath (Join-Path $root $ImportReport) -Raw | ConvertFrom-Json
if ($report.status -ne 'ready_for_visual_review') { throw 'A completed model import report is required.' }
$dest=[IO.Path]::GetFullPath((Join-Path $root $Destination))
if (!$dest.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Package output must be inside this project.' }
if (Test-Path -LiteralPath $dest) { throw 'Destination already exists. Choose a new directory; existing files will not be overwritten.' }
if (!$Node) { $Node=(Get-Command node -ErrorAction Stop).Source }
foreach ($folder in @('assets','behavior','tools/runtime/node','tools/runtime/blender','design','artifacts/three-reference')) {
    New-Item -ItemType Directory -Force -Path (Join-Path $dest $folder) | Out-Null
}
Copy-Item -LiteralPath (Join-Path $root 'build/desktop_pet.exe') -Destination $dest
Copy-Item -LiteralPath $report.model -Destination (Join-Path $dest ('assets/'+$report.name+'.glb'))
Copy-Item -LiteralPath $report.behavior -Destination (Join-Path $dest 'behavior/model.bt.xml')
Copy-Item -LiteralPath (Join-Path $root 'assets/alpha_cutout.fs') -Destination (Join-Path $dest 'assets')
foreach ($file in @('import-model.ps1','import_model.mjs','select_animations.mjs','normalize_skin_nodes.mjs','normalize_animation_names.mjs','sample_wiki_animations.mjs','blender_roundtrip.py','compare_wiki_pose.mjs')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file) -Destination (Join-Path $dest 'tools')
}
Copy-Item -LiteralPath (Join-Path $root 'design/hibiki-cafe-attention.bt.xml') -Destination (Join-Path $dest 'design')
Copy-Item -LiteralPath (Join-Path $root 'artifacts/three-reference/package') -Destination (Join-Path $dest 'artifacts/three-reference') -Recurse
Copy-Item -LiteralPath $Node -Destination (Join-Path $dest 'tools/runtime/node/node.exe')
Get-ChildItem -LiteralPath $BlenderDirectory | Copy-Item -Destination (Join-Path $dest 'tools/runtime/blender') -Recurse
[IO.File]::WriteAllLines((Join-Path $dest 'active-model.ini'),@('[Model]',("Path=assets/"+$report.name+".glb"),'Behavior=behavior/model.bt.xml'),[Text.Encoding]::Unicode)
# Keep the official runtime license alongside the shipped binary.
$nodeVersion=(& $Node --version).Trim()
Invoke-WebRequest -Uri ("https://raw.githubusercontent.com/nodejs/node/"+$nodeVersion+"/LICENSE") -OutFile (Join-Path $dest 'tools/runtime/node/LICENSE')
New-Item -ItemType Directory -Path (Join-Path $dest 'licenses') | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'vendor/raylib-6.0/LICENSE') -Destination (Join-Path $dest 'licenses/raylib.txt')
Copy-Item -LiteralPath (Join-Path $root 'vendor/BehaviorTree.CPP-4.8.0/LICENSE') -Destination (Join-Path $dest 'licenses/BehaviorTree.CPP.txt')
& $Node --version | Set-Content (Join-Path $dest 'tools/runtime/node/VERSION.txt')
@'
Double-click desktop_pet.exe. Middle-click the character and choose Import GLB.
Node and Blender runtimes are included; no separate development environment is needed.
The first conversion may take several minutes. Existing character stays until validation succeeds.
This package is an engineering prototype. Review Blender, Node, Three.js, raylib, BehaviorTree.CPP,
and model redistribution terms before publishing the package; keep their license files.
'@ | Set-Content (Join-Path $dest 'README.txt')
Write-Output "Portable folder: $dest"
