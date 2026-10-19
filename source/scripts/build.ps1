<#
.SYNOPSIS
Build TOU and the level tools using CMake. Run from any working directory.
.EXAMPLE
./source/scripts/build.ps1 -Architecture x64
.EXAMPLE
./source/scripts/build.ps1 -Architecture Win32 -Configuration RelWithDebInfo
#>
[CmdletBinding()]
param(
    [ValidateSet('Win32', 'x64', 'ARM64')][string]$Architecture = 'x64',
    [ValidateSet('Release', 'RelWithDebInfo', 'Debug')][string]$Configuration = 'Release',
    [string]$Generator,
    [string]$BuildDirectory,
    [ValidateRange(1, 256)][int]$Parallel = [Environment]::ProcessorCount
)
$ErrorActionPreference = 'Stop'
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$onWindows = [Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
$cmake = if ($cmakeCommand) { $cmakeCommand.Source } else { $null }
if (-not $cmake -and $onWindows) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $cmake = @(& $vswhere -latest -products '*' -find 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe') | Select-Object -First 1
    }
}
if (-not $cmake) { throw 'CMake 3.24+ is required. Install it or add it to PATH.' }
if (-not $Generator) { $Generator = if ($onWindows) { 'Visual Studio 17 2022' } else { 'Ninja' } }
if (-not $BuildDirectory) {
    $BuildDirectory = if ($onWindows) { "source/.build/$Architecture" } else { 'source/.build/native' }
}
if (-not [IO.Path]::IsPathRooted($BuildDirectory)) { $BuildDirectory = Join-Path $repositoryRoot $BuildDirectory }
$configureArguments = @('-S', $repositoryRoot, '-B', $BuildDirectory, '-G', $Generator, "-DCMAKE_BUILD_TYPE=$Configuration")
if ($onWindows -and $Generator.StartsWith('Visual Studio')) { $configureArguments += @('-A', $Architecture) }
& $cmake @configureArguments
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
& $cmake --build $BuildDirectory --config $Configuration --parallel $Parallel
if ($LASTEXITCODE -ne 0) { throw 'Build failed. If TOU is running, close it before rebuilding.' }
