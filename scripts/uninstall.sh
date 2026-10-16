#!/bin/sh
set -e

# light-git (lgit) Linux / macOS uninstaller

PREFIX="${PREFIX:-$HOME/.local}"
BINDIR="$PREFIX/bin"
TARGET="$BINDIR/lgit"

printf "%b\n" "\033[1;35mlight-git (lgit) Unix Uninstaller\033[0m"
printf "%b\n" "================================="

if [ -f "$TARGET" ]; then
    rm -f "$TARGET"
    printf "%b\n" "\033[32mSuccessfully removed $TARGET\033[0m"
else
    printf "%b\n" "Binary $TARGET not found."
fi

printf "%b\n" "Uninstallation complete. User repositories were preserved."
