param(
    [string]$Version = "dev",
    [string]$Platform = "windows-x86",
    [string]$ExecutablePath,
    [string]$OutputDirectory
)

$ErrorActionPreference = "Stop"

$repositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repositoryRoot "dist"
}

$distRoot = [System.IO.Path]::GetFullPath($OutputDirectory)
$safeVersion = $Version -replace '[^A-Za-z0-9._-]', '-'
$safePlatform = $Platform -replace '[^A-Za-z0-9._-]', '-'
$packageName = "Tunnels-of-Underworld-$safeVersion-$safePlatform"
$packageRoot = [System.IO.Path]::GetFullPath((Join-Path $distRoot $packageName))
$archivePath = [System.IO.Path]::GetFullPath((Join-Path $distRoot "$packageName.zip"))
$distPrefix = $distRoot.TrimEnd([System.IO.Path]::DirectorySeparatorChar) + [System.IO.Path]::DirectorySeparatorChar

if (-not $packageRoot.StartsWith($distPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to package outside the requested dist directory: $packageRoot"
}

if ([string]::IsNullOrWhiteSpace($ExecutablePath)) {
    $ExecutablePath = Join-Path $repositoryRoot "TOU.exe"
}
$resolvedExecutable = [System.IO.Path]::GetFullPath($ExecutablePath)
$executableName = [System.IO.Path]::GetFileName($resolvedExecutable)

$requiredDirectories = @(
    "data",
    "ggstuff",
    "help",
    "levels",
    "music",
    "sfx",
    "ships",
    "swap"
)

if (-not (Test-Path -LiteralPath $resolvedExecutable -PathType Leaf)) {
    throw "Required release executable is missing: $resolvedExecutable"
}

foreach ($relativePath in $requiredDirectories) {
    $sourcePath = Join-Path $repositoryRoot $relativePath
    if (-not (Test-Path -LiteralPath $sourcePath)) {
        throw "Required release input is missing: $relativePath"
    }
}

New-Item -ItemType Directory -Force -Path $distRoot | Out-Null
if (Test-Path -LiteralPath $packageRoot) {
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}
if (Test-Path -LiteralPath $archivePath) {
    Remove-Item -LiteralPath $archivePath -Force
}
New-Item -ItemType Directory -Path $packageRoot | Out-Null

Copy-Item -LiteralPath $resolvedExecutable -Destination (Join-Path $packageRoot $executableName)
foreach ($relativePath in $requiredDirectories) {
    Copy-Item -LiteralPath (Join-Path $repositoryRoot $relativePath) -Destination $packageRoot -Recurse
}

Compress-Archive -Path (Join-Path $packageRoot "*") -DestinationPath $archivePath -CompressionLevel Optimal
Write-Output $archivePath
