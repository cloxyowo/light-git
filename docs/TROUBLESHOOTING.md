# Troubleshooting Guide

Solutions and diagnostic steps for common build, runtime, and repository issues in `light-git`.

---

## 1. Build and Compilation Issues

### `CMake Error: Could not find compiler`
**Cause:** C++ compiler tools are not installed or configured in the system PATH.
**Solution:**
- **Windows:** Install Visual Studio 2022+ with the "Desktop development with C++" workload selected.
- **Linux:** Install `build-essential` or `gcc`/`g++`:
  ```bash
  sudo apt-get update && sudo apt-get install -y build-essential cmake
  ```
- **macOS:** Install Xcode command line tools:
  ```bash
  xcode-select --install
  ```

---

## 2. CLI and Runtime Issues

### `error: Author identity unknown`
**Symptoms:** Running `lgit commit` prints:
```text
error: Author identity unknown. Please set your name and email:
  lgit config user.name "Your Name"
  lgit config user.email "you@example.com"
```
**Solution:** Run the suggested configuration commands. To configure for all repositories:
```bash
lgit config --global user.name "Your Name"
lgit config --global user.email "you@example.com"
```

### `'lgit' is not recognized as an internal or external command`
**Cause:** The installation directory has not been added to your session's `PATH`.
**Solution:**
- Close and reopen your terminal.
- Or refresh your current session's PATH in PowerShell:
  ```powershell
  $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "User") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "Machine")
  ```

---

## 3. Repository State Issues

### `error: Your local changes would be overwritten by switch`
**Cause:** You have uncommitted modifications in your working tree that differ from the target branch.
**Solution:**
- To commit changes first:
  ```bash
  lgit add .
  lgit commit -m "WIP: save changes before switching"
  ```
- Or to discard changes:
  ```bash
  lgit restore .
  ```

### `error: Object integrity error: SHA-256 mismatch`
**Cause:** One or more files in `.light-git/objects/` have been corrupted on disk (e.g. disk sector error or external tampering).
**Solution:**
- Run `lgit fsck` to identify which object files are broken.
- If a backup exists, restore the broken object from backup.

### `error: Index file checksum mismatch`
**Cause:** The binary `.light-git/index` file was truncated or edited by an external tool.
**Solution:**
- You can safely regenerate the index from the HEAD commit:
  ```bash
  lgit reset --mixed HEAD
  ```
  This restores the index to the committed tree without deleting your working directory files.
