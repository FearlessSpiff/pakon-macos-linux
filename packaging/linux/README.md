# Pakon on Linux

The same app as the Mac one: the web service in its own window
(`pakon_linux.py`, pywebview on GTK WebKit2 or Qt WebEngine), or in your browser
when pywebview is missing or you pass `--browser`.

## Install

### Arch Linux

```sh
cd packaging/linux
makepkg -si
```

This builds the `feat/linux` branch of the fork named in `PKGBUILD`. To build
your own checkout instead:

```sh
PAKON_SRC="git+file://$PWD/../..#branch=$(git branch --show-current)" makepkg -si
```

The package puts the app in `/usr/lib/pakon`, adds the `pakon` command, a menu
entry and the udev rule. For the app window also install `python-pywebview`
(it uses `python-gobject` and `webkit2gtk-4.1`).

### Any distro, from the checkout

```sh
# Debian/Ubuntu: sudo apt install build-essential cmake pkg-config libusb-1.0-0-dev \
#                     python3-venv python3-gi gir1.2-webkit2-4.1
# Fedora:        sudo dnf install gcc cmake pkgconf libusb1-devel python3 \
#                     python3-gobject webkit2gtk4.1
packaging/linux/install.sh --udev
```

It builds the C tools in `build/`, makes a venv in `.venv` (with the system's
PyGObject for the window), installs `~/.local/bin/pakon` and a menu entry, and
with `--udev` the udev rule (asks for sudo). `install.sh --uninstall` removes the
command and menu entry.

## USB access

Linux gives a USB device to root only, unless a udev rule says otherwise.
`60-pakon.rules` gives the scanner (cold `0f05:f235` and warm `0f05:f135`) to
whoever is logged in at the machine. After installing it, switch the scanner off
and on. Without it Pakon shows the scanner as missing, and only `sudo` would work.

For a headless scanner server, give the device to a group instead (see the
comment in the rule) and run `uvicorn web.app:app --host 0.0.0.0 --port 8000`
as a user in that group.

## Where things go

| What | Where |
|---|---|
| EEPROM archives, settings | `~/.local/share/pakon` (`PAKON_DATA`) |
| Decoded roll, ~1 GB, emptied at launch | `~/.cache/pakon` (`PAKON_WORK`) |
| Log, crash log | `~/.local/state/pakon/pakon.log` |
| Scans | the work folder, `~/pakon` unless you pick another |

The EEPROM archive is the scanner's factory calibration and exists nowhere
else: keep a copy of `~/.local/share/pakon/eeprom` off the machine. If you used
`uvicorn` from the checkout before, your archive is in `backups/eeprom/`; copy it
to `~/.local/share/pakon/eeprom/` (or let Pakon read the scanner again).

**Show in Files** on the Export page opens the folder in your file manager. It
appears only when the page is open on the scanner's machine.
