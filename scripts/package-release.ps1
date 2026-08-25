param(
    [string]$Version = "dev",
    [string]$Platform = "windows-x86",
    [string]$ExecutablePath,
    [string]$LevelEditorDirectory,
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
$isWindowsPackage = $safePlatform.StartsWith(
    "windows-", [System.StringComparison]::OrdinalIgnoreCase)
$isMacPackage = $safePlatform.StartsWith(
    "macos-", [System.StringComparison]::OrdinalIgnoreCase)
$archiveExtension = if ($isWindowsPackage) { ".zip" } else { ".tar.gz" }
$archivePath = [System.IO.Path]::GetFullPath(
    (Join-Path $distRoot "$packageName$archiveExtension"))
$distPrefix = $distRoot.TrimEnd([System.IO.Path]::DirectorySeparatorChar) + [System.IO.Path]::DirectorySeparatorChar

if (-not $packageRoot.StartsWith($distPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to package outside the requested dist directory: $packageRoot"
}

if ([string]::IsNullOrWhiteSpace($ExecutablePath)) {
    $ExecutablePath = Join-Path $repositoryRoot "TOU.exe"
}
$resolvedExecutable = [System.IO.Path]::GetFullPath($ExecutablePath)
$executableName = [System.IO.Path]::GetFileName($resolvedExecutable)
if ([string]::IsNullOrWhiteSpace($LevelEditorDirectory)) {
    $LevelEditorDirectory = Join-Path (Split-Path -Parent $resolvedExecutable) "level-editor"
}
$resolvedLevelEditor = [System.IO.Path]::GetFullPath($LevelEditorDirectory)

$requiredDirectories = @(
    "data",
    "ggstuff",
    "help",
    "lang",
    "levels",
    "music",
    "sfx",
    "ships",
    "swap"
)

if (-not (Test-Path -LiteralPath $resolvedExecutable -PathType Leaf)) {
    throw "Required release executable is missing: $resolvedExecutable"
}
if (-not (Test-Path -LiteralPath $resolvedLevelEditor -PathType Container)) {
    throw "Staged level-editor directory is missing: $resolvedLevelEditor"
}

$toolSuffix = if ($isWindowsPackage) { ".exe" } else { "" }
$requiredEditorFiles = @(
    "tou-level$toolSuffix",
    "tou-level-editor$toolSuffix",
    "README.md",
    "docs/LEVEL_FORMAT.md",
    "docs/LEVEL_PALETTE.json",
    "docs/GG_LEVELS.md"
)
foreach ($relativePath in $requiredEditorFiles) {
    $editorInput = Join-Path $resolvedLevelEditor $relativePath
    if (-not (Test-Path -LiteralPath $editorInput -PathType Leaf)) {
        throw "Required level-editor runtime file is missing: $editorInput"
    }
}

$forbiddenEditorDirectories = @("src", "include", "editor", "tests", "fixtures")
foreach ($directory in $forbiddenEditorDirectories) {
    if (Test-Path -LiteralPath (Join-Path $resolvedLevelEditor $directory)) {
        throw "Level-editor staging unexpectedly contains source directory: $directory"
    }
}

foreach ($relativePath in $requiredDirectories) {
    $sourcePath = Join-Path $repositoryRoot $relativePath
    if (-not (Test-Path -LiteralPath $sourcePath)) {
        throw "Required release input is missing: $relativePath"
    }

    # Linux filesystems are case-sensitive. Runtime code uses normalized
    # lowercase asset paths, so reject packages that would work only on Windows.
    $nonPortablePaths = @(Get-ChildItem -LiteralPath $sourcePath -Recurse -Force |
        Where-Object { $_.Name -cne $_.Name.ToLowerInvariant() })
    if ($nonPortablePaths.Count -ne 0) {
        $badPaths = ($nonPortablePaths | ForEach-Object {
            $_.FullName.Substring($repositoryRoot.Length + 1)
        }) -join ", "
        throw "Runtime asset paths must be lowercase: $badPaths"
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

if ($isMacPackage) {
    $bundleRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $resolvedExecutable))
    if (-not $bundleRoot.EndsWith(".app", [System.StringComparison]::OrdinalIgnoreCase) -or
        -not (Test-Path -LiteralPath $bundleRoot -PathType Container)) {
        throw "macOS executable is not inside an app bundle: $resolvedExecutable"
    }
    $resourcesRoot = Join-Path $bundleRoot "Contents/Resources"
    foreach ($relativePath in $requiredDirectories) {
        $bundledPath = Join-Path $resourcesRoot $relativePath
        if (-not (Test-Path -LiteralPath $bundledPath -PathType Container)) {
            throw "Required macOS bundle input is missing: $bundledPath"
        }
    }
    $bundledIcon = Join-Path $resourcesRoot "icon.icns"
    if (-not (Test-Path -LiteralPath $bundledIcon -PathType Leaf)) {
        throw "Required macOS bundle icon is missing: $bundledIcon"
    }
    Copy-Item -LiteralPath $bundleRoot -Destination $packageRoot -Recurse
} else {
    Copy-Item -LiteralPath $resolvedExecutable -Destination (Join-Path $packageRoot $executableName)
    foreach ($relativePath in $requiredDirectories) {
        Copy-Item -LiteralPath (Join-Path $repositoryRoot $relativePath) -Destination $packageRoot -Recurse
    }
}
Copy-Item -LiteralPath $resolvedLevelEditor `
    -Destination (Join-Path $packageRoot "level-editor") -Recurse

if ($isWindowsPackage) {
    Compress-Archive -Path (Join-Path $packageRoot "*") `
        -DestinationPath $archivePath -CompressionLevel Optimal
} else {
    & tar -czf $archivePath -C $packageRoot .
    if ($LASTEXITCODE -ne 0) {
        throw "tar failed with exit code $LASTEXITCODE"
    }
}
Write-Output $archivePath
