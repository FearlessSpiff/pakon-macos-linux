#!/bin/bash
# Install Pakon for the current user from this checkout (any distro).
#
#   packaging/linux/install.sh            build, venv, `pakon` command, menu entry
#   packaging/linux/install.sh --udev     the same, plus the udev rule (sudo)
#   packaging/linux/install.sh --uninstall
#
# Needs: a C compiler, cmake, pkg-config, libusb-1.0 headers, python3 with
# venv. For the app window also the distro's PyGObject and WebKit2GTK 4.1
# (Arch: python-gobject webkit2gtk-4.1; Debian/Ubuntu: python3-gi
# gir1.2-webkit2-4.1); without them Pakon opens in the browser instead.
#
# The checkout stays where it is: `pakon` runs it in place, so a `git pull`
# and a rerun of this script update it.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
BIN="${XDG_BIN_HOME:-$HOME/.local/bin}"
SHARE="${XDG_DATA_HOME:-$HOME/.local/share}"
VENV="$REPO/.venv"
RULE=/etc/udev/rules.d/60-pakon.rules

step() { printf '\n==> %s\n' "$*"; }

if [ "${1:-}" = --uninstall ]; then
    rm -f "$BIN/pakon" "$SHARE/applications/pakon.desktop" \
          "$SHARE/icons/hicolor/512x512/apps/pakon.png"
    echo "Removed the pakon command and menu entry. Left alone: $VENV, $REPO/build,"
    echo "your scans, and $SHARE/pakon (EEPROM archives: keep a copy)."
    [ -f "$RULE" ] && echo "The udev rule stays; remove it with: sudo rm $RULE"
    exit 0
fi

step "C tools"
cmake -S "$REPO" -B "$REPO/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$REPO/build" -j"$(nproc 2>/dev/null || echo 2)"
ctest --test-dir "$REPO/build" --output-on-failure

step "Python environment ($VENV)"
# system site packages: the distro's PyGObject (the GTK window) is hard to pip
[ -x "$VENV/bin/python" ] || python3 -m venv --system-site-packages "$VENV"
"$VENV/bin/pip" install -q --upgrade pip
"$VENV/bin/pip" install -q -r "$REPO/web/requirements.txt" pywebview

step "pakon command and menu entry"
mkdir -p "$BIN" "$SHARE/applications" "$SHARE/icons/hicolor/512x512/apps"
cat > "$BIN/pakon" <<EOF
#!/bin/sh
exec "$VENV/bin/python" "$REPO/packaging/linux/pakon_linux.py" "\$@"
EOF
chmod +x "$BIN/pakon"
sed "s|^Exec=pakon|Exec=$BIN/pakon|" "$HERE/pakon.desktop" \
    > "$SHARE/applications/pakon.desktop"
cp "$REPO/website/pakon-icon.png" "$SHARE/icons/hicolor/512x512/apps/pakon.png"
command -v update-desktop-database >/dev/null \
    && update-desktop-database -q "$SHARE/applications" || true
command -v gtk-update-icon-cache >/dev/null \
    && gtk-update-icon-cache -q -t "$SHARE/icons/hicolor" 2>/dev/null || true

if [ "${1:-}" = --udev ]; then
    step "udev rule ($RULE, needs sudo)"
    sudo install -m 644 "$HERE/60-pakon.rules" "$RULE"
    sudo udevadm control --reload
    sudo udevadm trigger --subsystem-match=usb --attr-match=idVendor=0f05
fi

step "Done"
echo "Run 'pakon' (or pick Pakon in the app menu); 'pakon --browser' opens it in the browser."
case ":$PATH:" in *":$BIN:"*) ;; *) echo "Note: $BIN is not on your PATH." ;; esac
if [ ! -f "$RULE" ] && [ ! -f /usr/lib/udev/rules.d/60-pakon.rules ]; then
    echo "The scanner needs the udev rule for USB access without root:"
    echo "  $0 --udev"
fi
