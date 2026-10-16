<#
.SYNOPSIS
    light-git Windows Installer
.DESCRIPTION
    Installs light-git (lgit) into the user's local application directory and configures PATH.
.PARAMETER Prefix
    Target installation directory. Default is "$env:LOCALAPPDATA\Programs\light-git".
.PARAMETER SystemWide
    Install system-wide into "$env:ProgramFiles\light-git" (requires administrative privileges).
.PARAMETER DryRun
    Simulate the installation without writing files or modifying PATH.
.PARAMETER Quiet
    Suppress non-essential terminal output.
#>

[CmdletBinding()]
param (
    [string]$Prefix = "",
    [switch]$SystemWide,
    [switch]$DryRun,
    [switch]$Quiet
)

$ErrorActionPreference = "Stop"

function Write-Log {
    param([string]$Message, [string]$Color = "White")
    if (-not $Quiet) {
        Write-Host $Message -ForegroundColor $Color
    }
}

Write-Log "light-git (lgit) Windows Installer" -Color Magenta
Write-Log "=================================" -Color DarkGray

# 1. Architecture detection
$arch = if ([System.Environment]::Is64BitOperatingSystem) { "x64" } else { "x86" }
Write-Log "Detected Architecture: $arch" -Color Gray

# 2. Locate built executable
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$projectRoot = Split-Path -Parent $scriptDir

$candidatePaths = @(
    (Join-Path $projectRoot "build\Release\lgit.exe"),
    (Join-Path $projectRoot "build-release\Release\lgit.exe"),
    (Join-Path $projectRoot "build-release\lgit.exe"),
    (Join-Path $projectRoot "bin\lgit.exe")
)

$sourceExe = $null
foreach ($path in $candidatePaths) {
    if (Test-Path $path) {
        $sourceExe = $path
        break
    }
}

if (-not $sourceExe) {
    Write-Error "Could not find 'lgit.exe'. Please build the project first: cmake --build build --config Release"
    exit 1
}

Write-Log "Found source binary: $sourceExe" -Color Gray

# 3. Determine target directory
if (-not $Prefix) {
    if ($SystemWide) {
        $Prefix = Join-Path $env:ProgramFiles "light-git"
    } else {
        $Prefix = Join-Path $env:LOCALAPPDATA "Programs\light-git"
    }
}

$targetBinDir = Join-Path $Prefix "bin"
$targetExe = Join-Path $targetBinDir "lgit.exe"

Write-Log "Target Installation Directory: $targetBinDir" -Color Cyan

if ($DryRun) {
    Write-Log "[DryRun] Would create directory: $targetBinDir" -Color Yellow
    Write-Log "[DryRun] Would copy $sourceExe -> $targetExe" -Color Yellow
    Write-Log "[DryRun] Would add '$targetBinDir' to User PATH" -Color Yellow
    Write-Log "[DryRun] Dry run completed successfully." -Color Green
    exit 0
}

# 4. Create directory and copy binary
if (-not (Test-Path $targetBinDir)) {
    New-Item -ItemType Directory -Path $targetBinDir -Force | Out-Null
}

Copy-Item -Path $sourceExe -Destination $targetExe -Force
Write-Log "Installed lgit.exe -> $targetExe" -Color Green

# Copy uninstaller script to installation directory for convenience
$uninstallerSource = Join-Path $scriptDir "uninstall.ps1"
if (Test-Path $uninstallerSource) {
    Copy-Item -Path $uninstallerSource -Destination (Join-Path $Prefix "uninstall.ps1") -Force
}

# 5. PATH configuration
$scope = if ($SystemWide) { "Machine" } else { "User" }
$currentPath = [Environment]::GetEnvironmentVariable("Path", $scope)
$pathEntries = ($currentPath -split ';') | Where-Object { $_ -ne "" }

$alreadyInPath = $false
foreach ($entry in $pathEntries) {
    if ($entry.TrimEnd('\') -ieq $targetBinDir.TrimEnd('\')) {
        $alreadyInPath = $true
        break
    }
}

if (-not $alreadyInPath) {
    $newPath = if ($currentPath) { "$currentPath;$targetBinDir" } else { $targetBinDir }
    [Environment]::SetEnvironmentVariable("Path", $newPath, $scope)
    # Also update current session's PATH
    $env:Path = "$env:Path;$targetBinDir"
    Write-Log "Added '$targetBinDir' to $scope PATH." -Color Green
} else {
    Write-Log "'$targetBinDir' is already in $scope PATH." -Color Gray
}

# 6. Verify installed executable
try {
    $versionOutput = & $targetExe --version
    Write-Log "`nInstallation Verified:" -Color Green
    Write-Host $versionOutput -ForegroundColor Cyan
    Write-Log "light-git installed successfully!" -Color Magenta
    Write-Log "Restart any open terminals or run: `$env:Path = [System.Environment]::GetEnvironmentVariable('Path', 'User')" -Color DarkGray
} catch {
    Write-Error "Failed to execute installed binary: $_"
    exit 1
}

exit 0
