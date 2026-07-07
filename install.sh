#!/usr/bin/env sh
# install.sh (root) - compatibility redirect.
#
# The canonical Torvik installer now lives at linux/install.sh. This thin shim
# stays at the repository root so older bookmarked one-liners keep working:
#
#   curl -fsSL https://raw.githubusercontent.com/torvik-lang/torvik/main/install.sh | sh
#
# It simply fetches and runs the canonical Linux installer, forwarding any
# TORVIK_VERSION you set (so `rune update vX` and pinned installs still work).
# The current recommended command is:
#
#   curl -fsSL https://raw.githubusercontent.com/torvik-lang/torvik/main/linux/install.sh | sh
#
# Windows users: use windows/install.ps1 instead (see the README).
set -e

CANON="https://raw.githubusercontent.com/torvik-lang/torvik/main/linux/install.sh"

# On Windows shells (Git Bash / MSYS), point the user at the PowerShell installer.
case "$(uname -s 2>/dev/null | tr '[:upper:]' '[:lower:]')" in
  mingw*|msys*|cygwin*)
    echo "It looks like you're on Windows. Use the PowerShell installer instead:"
    echo ""
    echo "  iwr -useb https://raw.githubusercontent.com/torvik-lang/torvik/main/windows/install.ps1 | iex"
    echo ""
    exit 1 ;;
esac

if command -v curl >/dev/null 2>&1; then
    curl -fsSL "$CANON" | sh
elif command -v wget >/dev/null 2>&1; then
    wget -qO- "$CANON" | sh
else
    echo "error: need curl or wget to fetch the installer."
    echo "  See https://github.com/torvik-lang/torvik for manual steps."
    exit 1
fi
