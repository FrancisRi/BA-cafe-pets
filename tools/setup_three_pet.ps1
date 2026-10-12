$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$sdk=Join-Path $root 'vendor/webview2-1.0.3650.58'
$archive=Join-Path $sdk 'sdk.zip'
New-Item -ItemType Directory -Force -Path $sdk | Out-Null
if(!(Test-Path -LiteralPath $archive)) {
    Invoke-WebRequest -Uri 'https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/1.0.3650.58/microsoft.web.webview2.1.0.3650.58.nupkg' -OutFile $archive
}
if((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne '911A472128C82AC8BAA0C486C23342CC9DD6E7DC50D754E676726642CA065C60') {throw 'WebView2 SDK checksum mismatch'}
Expand-Archive -LiteralPath $archive -DestinationPath $sdk -Force
if(!(Test-Path -LiteralPath (Join-Path $root 'artifacts/three-reference/package/build/three.module.js'))) { & (Join-Path $PSScriptRoot 'setup_wiki_reference.ps1') }
Write-Output 'Three.js 0.185.1 and WebView2 SDK ready. Runtime must be present on the target PC.'
