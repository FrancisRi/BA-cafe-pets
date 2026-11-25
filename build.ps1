param([int]$Jobs=4,[ValidatePattern('^[A-Za-z0-9_-]+$')][string]$ExecutableName='BA-Cafe-Pets',[switch]$Release,[switch]$NativeOnly)
$ErrorActionPreference='Stop'
$cmake=(Get-Command cmake -ErrorAction SilentlyContinue).Source
if(!$cmake){$cmake=(Get-ChildItem -Path "$PSScriptRoot/.tools/cmake-*/bin/cmake.exe" -ErrorAction SilentlyContinue | Sort-Object FullName -Descending | Select-Object -First 1).FullName}
if(!$cmake){$cmake='C:/Program Files/CMake/bin/cmake.exe'}
$compiler=(Get-Command g++ -ErrorAction SilentlyContinue).Source
if(!$compiler -and (Test-Path -LiteralPath "$PSScriptRoot/.tools/w64devkit/bin/g++.exe")){$compiler="$PSScriptRoot/.tools/w64devkit/bin/g++.exe"}
if(!$compiler){$compiler=(Get-ChildItem -Path "$env:LOCALAPPDATA/Microsoft/WinGet/Packages/BrechtSanders.WinLibs.*/mingw64/bin/c++.exe" -ErrorAction SilentlyContinue | Select-Object -First 1).FullName}
$ninja=(Get-Command ninja -ErrorAction SilentlyContinue).Source
if(!$ninja -and (Test-Path -LiteralPath "$PSScriptRoot/.tools/ninja/ninja.exe")){$ninja="$PSScriptRoot/.tools/ninja/ninja.exe"}
if(!$ninja){$ninja=(Get-ChildItem -Path "$env:LOCALAPPDATA/Microsoft/WinGet/Packages/Ninja-build.Ninja_*/ninja.exe" -ErrorAction SilentlyContinue | Select-Object -First 1).FullName}
if(!$compiler -or !$ninja){throw 'Install a C++20 toolchain and Ninja before building; the built program needs neither.'}
$env:PATH=(Split-Path $compiler)+';'+(Split-Path $ninja)+';'+$env:PATH
& $cmake -S $PSScriptRoot -B (Join-Path $PSScriptRoot 'build') -G Ninja '-DCMAKE_BUILD_TYPE=Release' "-DCMAKE_CXX_COMPILER=$compiler" "-DCMAKE_MAKE_PROGRAM=$ninja" "-DBA_NATIVE_EXECUTABLE_NAME=$ExecutableName"
if($LASTEXITCODE -ne 0){throw 'Native CMake configure failed'}
$targets=@('ba_cafe_lite_native')
if($Release){$targets+='ba_cafe_lite_release_launcher'}
& $cmake --build (Join-Path $PSScriptRoot 'build') --target @targets -j $Jobs
if($LASTEXITCODE -ne 0){throw 'Native build failed'}
if(!$NativeOnly){
    $dotnet=Join-Path $PSScriptRoot '.tools/dotnet-winui/dotnet.exe'
    if(!(Test-Path -LiteralPath $dotnet)){$dotnet=(Get-Command dotnet -ErrorAction SilentlyContinue).Source}
    if(!$dotnet -or !(& $dotnet --list-sdks)){throw 'Install the .NET 8 SDK to build the WinUI 3 interface'}
    $env:DOTNET_CLI_TELEMETRY_OPTOUT='1'
    $env:DOTNET_GENERATE_ASPNET_CERTIFICATE='false'
    $nuget=Join-Path $PSScriptRoot '.tools/winui-nuget'
    if(!$env:NUGET_PACKAGES -and (Test-Path -LiteralPath $nuget)){$env:NUGET_PACKAGES=$nuget}
    & $dotnet publish (Join-Path $PSScriptRoot 'winui/BA-Cafe-Settings.csproj') -c Release -o (Join-Path $PSScriptRoot 'build/bin/winui')
    if($LASTEXITCODE -ne 0){throw 'WinUI 3 interface build failed'}
}
Write-Output (Join-Path $PSScriptRoot "build/bin/$ExecutableName.exe")
