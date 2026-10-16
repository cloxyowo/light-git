<#
.SYNOPSIS
    light-git Windows Installer
.DESCRIPTION
    Installs light-git (lgit) into the user's local application directory and configures PATH.
    Supports both local execution (from source repo) and one-liner remote web execution:
      irm https://your-website.com/install.ps1 | iex
.PARAMETER Prefix
    Target installation directory. Default is "$env:LOCALAPPDATA\Programs\light-git".
.PARAMETER BaseUrl
    Base URL hosting the precompiled lgit.exe binary when installed via the web.
    Default points to GitHub Releases latest.
.PARAMETER DownloadUrl
    Direct download URL for lgit.exe (overrides BaseUrl).
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
    [string]$BaseUrl = "https://github.com/cloxyowo/light-git/releases/latest/download",
    [string]$DownloadUrl = "",
    [switch]$SystemWide,
    [switch]$DryRun,
    [switch]$Quiet
)

$ErrorActionPreference = "Stop"

# Enable modern TLS protocols
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12 -bor [Net.SecurityProtocolType]::Tls13

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

# 2. Determine target directory
if (-not $Prefix) {
    if ($SystemWide) {
        $Prefix = Join-Path $env:ProgramFiles "light-git"
    } else {
        $Prefix = Join-Path $env:LOCALAPPDATA "Programs\light-git"
    }
}

$targetBinDir = Join-Path $Prefix "bin"
$targetExe = Join-Path $targetBinDir "lgit.exe"
$uninstallerPath = Join-Path $Prefix "uninstall.ps1"

Write-Log "Target Installation Directory: $targetBinDir" -Color Cyan

# 3. Locate or Download executable
$sourceExe = $null
$scriptPath = $null
try {
    if ($MyInvocation -and $MyInvocation.MyCommand -and $MyInvocation.MyCommand.Path) {
        $scriptPath = $MyInvocation.MyCommand.Path
    }
} catch {}

if ($scriptPath) {
    $scriptDir = Split-Path -Parent $scriptPath
    $projectRoot = Split-Path -Parent $scriptDir
    $candidatePaths = @(
        (Join-Path $projectRoot "build\Release\lgit.exe"),
        (Join-Path $projectRoot "build-release\Release\lgit.exe"),
        (Join-Path $projectRoot "build-release\lgit.exe"),
        (Join-Path $projectRoot "bin\lgit.exe"),
        (Join-Path $scriptDir "lgit.exe")
    )

    foreach ($path in $candidatePaths) {
        if (Test-Path $path) {
            $sourceExe = $path
            break
        }
    }
}

$isWebInstall = ($null -eq $sourceExe)

if ($isWebInstall) {
    if (-not $DownloadUrl) {
        $DownloadUrl = "$BaseUrl/lgit.exe"
    }
    Write-Log "Mode: Web Install" -Color Magenta
    Write-Log "Download URL: $DownloadUrl" -Color Gray
} else {
    Write-Log "Mode: Local Install" -Color Green
    Write-Log "Found local binary: $sourceExe" -Color Gray
}

if ($DryRun) {
    Write-Log "[DryRun] Target Directory: $targetBinDir" -Color Yellow
    if ($isWebInstall) {
        Write-Log "[DryRun] Would download: $DownloadUrl -> $targetExe" -Color Yellow
    } else {
        Write-Log "[DryRun] Would copy: $sourceExe -> $targetExe" -Color Yellow
    }
    Write-Log "[DryRun] Would configure User PATH: $targetBinDir" -Color Yellow
    Write-Log "[DryRun] Would generate uninstaller: $uninstallerPath" -Color Yellow
    Write-Log "[DryRun] Completed successfully." -Color Green
    exit 0
}

# 4. Create target directory
if (-not (Test-Path $targetBinDir)) {
    New-Item -ItemType Directory -Path $targetBinDir -Force | Out-Null
}

# 5. Deploy binary
if ($isWebInstall) {
    Write-Log "Downloading lgit.exe..." -Color Cyan
    try {
        $webClient = New-Object System.Net.WebClient
        $webClient.DownloadFile($DownloadUrl, $targetExe)
    } catch {
        # Fallback to Invoke-WebRequest
        try {
            Invoke-WebRequest -Uri $DownloadUrl -OutFile $targetExe -UseBasicParsing
        } catch {
            Write-Error "Failed to download lgit.exe from ${DownloadUrl} - $_"
            exit 1
        }
    }
} else {
    Copy-Item -Path $sourceExe -Destination $targetExe -Force
}

Write-Log "Installed executable -> $targetExe" -Color Green

# 6. Generate Self-Contained Uninstaller
$uninstallerContent = @"
<#
.SYNOPSIS
    light-git Windows Uninstaller
#>
[CmdletBinding()]
param (
    [switch]`$Quiet
)

`$Prefix = Split-Path -Parent `$MyInvocation.MyCommand.Definition
if (-not `$Prefix) {
    `$Prefix = "$Prefix"
}
`$targetBinDir = Join-Path `$Prefix "bin"

if (-not `$Quiet) {
    Write-Host "Uninstalling light-git from `$Prefix..." -ForegroundColor Magenta
}

# Clean PATH
foreach (`$scope in @("User", "Machine")) {
    try {
        `$currentPath = [Environment]::GetEnvironmentVariable("Path", `$scope)
        if (`$currentPath) {
            `$entries = (`$currentPath -split ';') | Where-Object { `$_ -ne "" -and `$_.TrimEnd('\') -ine `$targetBinDir.TrimEnd('\') -and `$_.TrimEnd('\') -ine `$Prefix.TrimEnd('\') }
            `$newPath = `$entries -join ';'
            [Environment]::SetEnvironmentVariable("Path", `$newPath, `$scope)
        }
    } catch {
        # Ignore security errors if uninstaller is run without admin privileges for Machine scope
    }
}

# Clean directory
if (Test-Path `$Prefix) {
    Remove-Item -Path `$Prefix -Recurse -Force
}

if (-not `$Quiet) {
    Write-Host "light-git uninstalled successfully. User repositories were preserved." -ForegroundColor Green
}
"@

Set-Content -Path $uninstallerPath -Value $uninstallerContent -Force
Write-Log "Created uninstaller -> $uninstallerPath" -Color Gray

# 7. PATH Configuration
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

# 8. Verify installed executable
try {
    $versionOutput = & $targetExe --version
    Write-Log "`nInstallation Verified:" -Color Green
    Write-Host $versionOutput -ForegroundColor Cyan
    Write-Log "light-git installed successfully!" -Color Magenta
    Write-Log "Run 'lgit --help' to get started." -Color White
    Write-Log "To uninstall, run: & `"$uninstallerPath`"" -Color DarkGray
} catch {
    Write-Error "Failed to execute installed binary: $_"
    exit 1
}

exit 0
