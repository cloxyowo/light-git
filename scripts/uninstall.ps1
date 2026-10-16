<#
.SYNOPSIS
    light-git Windows Uninstaller
.DESCRIPTION
    Removes light-git (lgit) from the system and cleans the PATH environment variable.
    Preserves user repositories and user configurations.
.PARAMETER Prefix
    Target installation directory to remove. Default is "$env:LOCALAPPDATA\Programs\light-git".
.PARAMETER SystemWide
    Uninstall system-wide installation (requires administrative privileges).
.PARAMETER DryRun
    Simulate removal without deleting files or modifying PATH.
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

Write-Log "light-git (lgit) Windows Uninstaller" -Color Magenta
Write-Log "===================================" -Color DarkGray

if (-not $Prefix) {
    if ($SystemWide) {
        $Prefix = Join-Path $env:ProgramFiles "light-git"
    } else {
        $Prefix = Join-Path $env:LOCALAPPDATA "Programs\light-git"
    }
}

$targetBinDir = Join-Path $Prefix "bin"

Write-Log "Target Installation to Remove: $Prefix" -Color Gray

# 1. Clean PATH
$scope = if ($SystemWide) { "Machine" } else { "User" }
$currentPath = [Environment]::GetEnvironmentVariable("Path", $scope)

if ($currentPath) {
    $pathEntries = ($currentPath -split ';') | Where-Object { $_ -ne "" }
    $filteredEntries = @()
    $removedFromPath = $false

    foreach ($entry in $pathEntries) {
        if ($entry.TrimEnd('\') -ieq $targetBinDir.TrimEnd('\') -or $entry.TrimEnd('\') -ieq $Prefix.TrimEnd('\')) {
            $removedFromPath = $true
        } else {
            $filteredEntries += $entry
        }
    }

    if ($removedFromPath) {
        if ($DryRun) {
            Write-Log "[DryRun] Would remove '$targetBinDir' from $scope PATH." -Color Yellow
        } else {
            $newPath = $filteredEntries -join ';'
            [Environment]::SetEnvironmentVariable("Path", $newPath, $scope)
            Write-Log "Removed '$targetBinDir' from $scope PATH." -Color Green
        }
    } else {
        Write-Log "'$targetBinDir' not found in $scope PATH." -Color Gray
    }
}

# 2. Remove files
if (Test-Path $Prefix) {
    if ($DryRun) {
        Write-Log "[DryRun] Would delete directory: $Prefix" -Color Yellow
    } else {
        try {
            Remove-Item -Path $Prefix -Recurse -Force
            Write-Log "Removed installation directory: $Prefix" -Color Green
        } catch {
            Write-Error "Failed to delete directory '$Prefix': $_"
            exit 1
        }
    }
} else {
    Write-Log "Directory '$Prefix' does not exist." -Color Gray
}

Write-Log "`nUninstallation complete. User repositories and configurations were preserved." -Color Magenta
exit 0
