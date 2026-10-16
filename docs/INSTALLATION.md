# Installation Guide

`light-git` (`lgit`) is designed to install with zero runtime dependencies and an ultra-compact footprint (~380 KB). Users do not need to download or build the entire project.

---

## 1. Instant One-Liner Web Installation

### Windows (PowerShell)

Open PowerShell and run:

```powershell
irm https://raw.githubusercontent.com/cloxyowo/light-git/main/scripts/install.ps1 | iex
```

This:
1. Detects your system architecture (`x64`).
2. Downloads precompiled `lgit.exe`.
3. Installs it into `%LOCALAPPDATA%\Programs\light-git\bin\lgit.exe`.
4. Automatically adds the directory to your User `PATH`.
5. Creates a self-contained uninstaller script.
6. Verifies that `lgit --version` runs properly.

#### Customizing the One-Liner
You can specify custom parameters directly in the one-liner:
```powershell
& ([scriptblock]::Create((irm https://raw.githubusercontent.com/cloxyowo/light-git/main/scripts/install.ps1))) -Prefix "C:\Tools\light-git"
```

### Linux & macOS (curl)

Open your terminal and run:

```bash
curl -fsSL https://raw.githubusercontent.com/cloxyowo/light-git/main/scripts/install.sh | sh
```

This downloads the binary for your OS and architecture into `$HOME/.local/bin/lgit` and sets executable permissions.

---

## 2. Self-Hosting on Your Own Website

You can easily host `light-git` on your own website, blog, or CDN so users can install it from your custom domain:

### Step 1: Upload the Files
Upload the following two files to your web server:
1. `lgit.exe` (from `build/Release/lgit.exe` or GitHub Releases).
2. `install.ps1` (from `scripts/install.ps1`).

*Optional (for Linux/macOS users):*
Upload `lgit-linux-x64`, `lgit-macos-arm64`, and `install.sh`.

### Step 2: Share Your Custom One-Liner
Users can now install `lgit` directly from your domain with:

```powershell
irm https://your-website.com/install.ps1 | iex
```

If `lgit.exe` is hosted in the same directory as `install.ps1`, you can set the default URL by changing the `$BaseUrl` line at the top of `install.ps1`:
```powershell
[string]$BaseUrl = "https://your-website.com"
```
Or users can specify your binary URL explicitly:
```powershell
& ([scriptblock]::Create((irm https://your-website.com/install.ps1))) -DownloadUrl "https://your-website.com/lgit.exe"
```

---

## 3. Local Installation (From Cloned Repository)

If you have already cloned the repository and built the binary locally:

### Windows (PowerShell)
```powershell
.\scripts\install.ps1
```

Options:
- `-Prefix <path>`: Custom installation folder.
- `-DryRun`: Simulate without writing files or modifying PATH.
- `-Quiet`: Suppress non-essential logs.
- `-SystemWide`: Install system-wide into `C:\Program Files\light-git` (requires Administrator).

### Linux & macOS (Shell)
```bash
sh scripts/install.sh
```

---

## 4. Building from Source

```bash
# Configure Release build
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Build executable and library
cmake --build build --config Release

# Run automated tests
ctest --test-dir build -C Release --output-on-failure
```

---

## 5. Uninstallation

### Windows

Run the uninstaller created during installation:

```powershell
& "$env:LOCALAPPDATA\Programs\light-git\uninstall.ps1"
```

Or from the repository:
```powershell
.\scripts\uninstall.ps1
```

The uninstaller removes the `lgit.exe` binary, deletes the installation directory, and cleans the `PATH` environment variable. **It never modifies your repositories or user configurations.**

### Linux / macOS

```bash
sh ~/.local/uninstall-lgit.sh
```
