$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$reference = Join-Path $root 'artifacts\three-reference'
New-Item -ItemType Directory -Force -Path $reference | Out-Null
$archive = Join-Path $reference 'three.tgz'
Invoke-WebRequest -Uri 'https://registry.npmjs.org/three/-/three-0.185.1.tgz' -OutFile $archive
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne 'A2143F5BF978BD3470A51024B2B6BDD581913BA8F36FF1538D433F3A95ADF2DF') {
    throw 'Three.js reference archive checksum mismatch'
}
tar -xf $archive -C $reference
if ($LASTEXITCODE -ne 0) { throw 'Cannot extract Three.js reference' }
Write-Output 'Wiki reference ready: Three.js 0.185.1 (MIT), resource tools only.'
