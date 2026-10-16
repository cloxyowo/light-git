# Installation Guide

`light-git` (`lgit`) is designed to install with zero runtime dependencies and a compact single-digit megabyte footprint (the executable is only ~380 KB).

---

## 1. Windows Installation

### Option A: Automated PowerShell Installer (Recommended)

1. Open PowerShell.
2. Navigate to the repository root.
3. Run the installer script:

```powershell
.\scripts\install.ps1
```

By default, this installs `lgit.exe` into:
```
%LOCALAPPDATA%\Programs\light-git\bin\lgit.exe
```
and automatically appends this path to your User `PATH` environment variable.

#### Installer Options

- **`-Prefix <path>`**: Specify a custom installation directory:
  ```powershell
  .\scripts\install.ps1 -Prefix "C:\Tools\light-git"
  ```
- **`-DryRun`**: Test what the installer would do without modifying your system:
  ```powershell
  .\scripts\install.ps1 -DryRun
  ```
- **`-Quiet`**: Suppress output during automated CI/CD scripts.
- **`-SystemWide`**: Install into `C:\Program Files\light-git` (requires Administrator prompt).

### Option B: Manual Windows Build

If building from source:

```powershell
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The resulting binary will be located at `build\Release\lgit.exe`. Copy this binary to any folder in your `PATH`.

---

## 2. Linux & macOS Installation

### Option A: Shell Installer Script

```bash
sh scripts/install.sh
```

By default, this installs `lgit` to `$HOME/.local/bin/lgit`.

To install to a custom directory (e.g. `/usr/local/bin`):
```bash
PREFIX=/usr/local sh scripts/install.sh
```

### Option B: CMake Install Target

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
sudo cmake --install build
```

---

## 3. Uninstallation

### Windows

Run the uninstaller script:

```powershell
.\scripts\uninstall.ps1
```

Or run the uninstaller placed in your installation directory:
```powershell
& "$env:LOCALAPPDATA\Programs\light-git\uninstall.ps1"
```

The uninstaller removes the executable and cleans the `PATH` environment variable. **It never deletes user repositories or configuration files.**

### Linux / macOS

```bash
sh scripts/uninstall.sh
```

Or manually remove the installed binary:
```bash
rm -f "$HOME/.local/bin/lgit"
```
