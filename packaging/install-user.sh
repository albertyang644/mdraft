#!/usr/bin/env bash
#
# install-user.sh - install mdraft (binary, icon, desktop entry) for the
# current user under ~/.local (no root required). Makes mdraft launchable
# from the KDE application menu and desktop.
#
set -euo pipefail

BUILD_DIR="${1:-build}"
APPS="$HOME/.local/share/applications"
ICONDIR="$HOME/.local/share/icons/hicolor"

echo "Installing mdraft for user $USER..."

cmake --install "$BUILD_DIR" --prefix "$HOME/.local"

# Refresh caches (best-effort, user-level)
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
  gtk-update-icon-cache -f -t "$ICONDIR" >/dev/null 2>&1 || true
fi
if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database "$APPS" >/dev/null 2>&1 || true
fi

echo "Done. mdraft is now available in the KDE menu / desktop."
echo "You may need to log out/in (or run 'kbuildsycoca6' if KDE) for it to appear."
