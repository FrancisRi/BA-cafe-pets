param([int]$Jobs=4,[ValidatePattern('^[A-Za-z0-9_-]+$')][string]$ExecutableName='ba_cafe_lite_native',[switch]$Release)
$ErrorActionPreference='Stop'
$cmake=(Get-Command cmake -ErrorAction SilentlyContinue).Source
if(!$cmake){$cmake='C:/Program Files/CMake/bin/cmake.exe'}
$compiler=(Get-Command g++ -ErrorAction SilentlyContinue).Source
if(!$compiler){$compiler=(Get-ChildItem -Path "$env:LOCALAPPDATA/Microsoft/WinGet/Packages/BrechtSanders.WinLibs.*/mingw64/bin/c++.exe" -ErrorAction SilentlyContinue | Select-Object -First 1).FullName}
$ninja=(Get-Command ninja -ErrorAction SilentlyContinue).Source
if(!$ninja){$ninja=(Get-ChildItem -Path "$env:LOCALAPPDATA/Microsoft/WinGet/Packages/Ninja-build.Ninja_*/ninja.exe" -ErrorAction SilentlyContinue | Select-Object -First 1).FullName}
if(!$compiler -or !$ninja){throw 'Install a C++20 toolchain and Ninja before building; the built program needs neither.'}
$env:PATH=(Split-Path $compiler)+';'+(Split-Path $ninja)+';'+$env:PATH
& $cmake -S $PSScriptRoot -B (Join-Path $PSScriptRoot 'build') -G Ninja '-DCMAKE_BUILD_TYPE=Release' "-DCMAKE_CXX_COMPILER=$compiler" "-DCMAKE_MAKE_PROGRAM=$ninja" "-DBA_NATIVE_EXECUTABLE_NAME=$ExecutableName"
if($LASTEXITCODE -ne 0){throw 'Native CMake configure failed'}
$targets=@('ba_cafe_lite_native')
if($Release){$targets+='ba_cafe_lite_release_launcher'}
& $cmake --build (Join-Path $PSScriptRoot 'build') --target @targets -j $Jobs
if($LASTEXITCODE -ne 0){throw 'Native build failed'}
Write-Output (Join-Path $PSScriptRoot "build/bin/$ExecutableName.exe")
