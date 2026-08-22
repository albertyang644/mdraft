#!/usr/bin/env bash
#
# install-user.sh - install mdraft (binary, icon, desktop entry) for the
# current user under ~/.local (no root required). Makes mdraft launchable
# from the KDE application menu and desktop.
#
set -euo pipefail

BIN_SRC="${1:-build/mdraft}"          # path to the built binary (default: build/mdraft)
BINDIR="$HOME/.local/bin"
APPS="$HOME/.local/share/applications"
ICONDIR="$HOME/.local/share/icons/hicolor"
ICON_SRC_DIR="assets/icons/hicolor"

echo "Installing mdraft for user $USER..."

# 1) Binary
mkdir -p "$BINDIR"
install -m 0755 "$BIN_SRC" "$BINDIR/mdraft"
echo "  binary -> $BINDIR/mdraft"

# 2) Icons (all sizes)
for size in 16 32 48 64 128 256 512; do
  install -d "$ICONDIR/${size}x${size}/apps"
  if [[ -f "$ICON_SRC_DIR/${size}x${size}/apps/mdraft.png" ]]; then
    install -m 0644 "$ICON_SRC_DIR/${size}x${size}/apps/mdraft.png" "$ICONDIR/${size}x${size}/apps/mdraft.png"
  fi
done
# Scalable SVG icon (preferred by KDE/GTK when the theme supports it)
install -d "$ICONDIR/scalable/apps"
if [[ -f "assets/icons/mdraft.svg" ]]; then
  install -m 0644 "assets/icons/mdraft.svg" "$ICONDIR/scalable/apps/mdraft.svg"
fi
echo "  icons -> $ICONDIR"

# 3) Desktop entry (Exec points at the installed binary)
install -d "$APPS"
sed "s|@BINDIR@|$BINDIR|g" packaging/mdraft.desktop > "$APPS/mdraft.desktop"
chmod 0644 "$APPS/mdraft.desktop"
echo "  desktop -> $APPS/mdraft.desktop"

# 4) Refresh caches (best-effort, user-level)
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
  gtk-update-icon-cache -f -t "$ICONDIR" >/dev/null 2>&1 || true
fi
if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database "$APPS" >/dev/null 2>&1 || true
fi

echo "Done. mdraft is now available in the KDE menu / desktop."
echo "You may need to log out/in (or run 'kbuildsycoca6' if KDE) for it to appear."
