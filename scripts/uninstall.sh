#!/bin/sh
set -e

# light-git (lgit) Linux / macOS uninstaller

PREFIX="${PREFIX:-$HOME/.local}"
BINDIR="$PREFIX/bin"
TARGET="$BINDIR/lgit"

echo "\033[1;35mlight-git (lgit) Unix Uninstaller\033[0m"
echo "================================="

if [ -f "$TARGET" ]; then
    rm -f "$TARGET"
    echo "\033[32mSuccessfully removed $TARGET\033[0m"
else
    echo "Binary $TARGET not found."
fi

echo "Uninstallation complete. User repositories were preserved."
