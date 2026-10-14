# Pakon.app (macOS)

A double-clickable app for people who don't use the command line. It runs the
web service from `web/` and shows it in its own window with
[pywebview](https://pywebview.flowrl.com) (WKWebView); closing the window quits.
PyInstaller freezes it with its own Python, the C tools, libusb and the
firmware, so the Mac needs nothing installed. Apple silicon, macOS 13 or later.

| File                 | What it is |
|----------------------|------------|
| `pakon_app.py`       | Entry point: web service on a free localhost port + the window |
| `pakon.spec`         | PyInstaller spec: what goes in the bundle |
| `build.sh`           | Builds libusb and the C tools for macOS 13, freezes, signs, makes the `.dmg`, notarizes |
| `requirements.txt`   | Build requirements (the web service's, plus pywebview and PyInstaller) |
| `entitlements.plist` | Hardened-runtime exceptions for Python, unicorn and the bundled libraries |
| `make_icon.py`       | Draws the app icon |

## Build

```sh
packaging/macos/build.sh 0.2.0        # version number
```

Output goes to `dist/macos/`: `Pakon.app` and `Pakon.dmg`. Downloads and
intermediate files are cached in `packaging/macos/.cache/` (both git-ignored).
Needs Xcode's command-line tools, CMake and pkg-config. Python and libusb are
downloaded at pinned versions and checked against their SHA-256.

`build.sh` fails if any binary in the bundle needs a newer macOS than 13: wheels
are fetched for macOS 13, and libusb is built from source because Homebrew's
targets the running macOS.

To try the window without building: `python packaging/macos/pakon_app.py`
(with `web/requirements.txt` and `pywebview` installed).

## Signing and notarization

`build.sh` signs with the first **Developer ID Application** certificate in the
keychain (or `PAKON_SIGN_IDENTITY`). Without one it signs ad hoc: the app runs
on the Mac that built it, but other Macs will refuse it.

Notarizing needs a stored App Store Connect login, once per Mac:

```sh
xcrun notarytool store-credentials pakon-notary \
    --apple-id YOU@EXAMPLE.COM --team-id T928P2TAKQ
```

Then:

```sh
PAKON_NOTARY_PROFILE=pakon-notary packaging/macos/build.sh 0.2.0
```

This submits the `.dmg`, waits for Apple, staples the ticket and checks it with
`spctl`. Upload `dist/macos/Pakon.dmg` to a GitHub release under that exact
name: the website's Download button points at
`releases/latest/download/Pakon.dmg`.

### Crash reports and usage statistics (Rybbit)

The app can send opt-in crash reports and usage statistics to a
[Rybbit](https://rybbit.com) instance (`web/telemetry.py`). Where they go is
set when building, never in the code, so a build anyone makes from this repo
sends nothing and never asks. For a release, copy `release.env.example` to
`release.env` (git-ignored) and fill in:

| Variable | What |
|---|---|
| `PAKON_RYBBIT_URL` | the Rybbit instance, e.g. `https://rybbit.example.com` |
| `PAKON_RYBBIT_SITE_ID` | the Rybbit site's ID |
| `PAKON_RYBBIT_DOMAIN` | the domain the Rybbit site was created with |

`build.sh` reads `release.env` (or the same variables from the environment),
prints `reports: on` or `reports: off`, and writes them into the app's
`Info.plist`; all three are needed. No API key: the app sends to Rybbit's
public tracking endpoint, and a key would be readable by anyone with the app.

Installed apps check the latest release on launch (and every 6 hours) and
show a banner when it is newer than their own version (`web/update.py`), so
tag releases `vX.Y.Z` with the same number passed to `build.sh`, and do not
mark them draft or pre-release unless they should stay hidden. Users can turn
the check off with `PAKON_UPDATE_CHECK=0`.

## Where the app keeps things

- EEPROM archives: `~/Library/Application Support/Pakon/eeprom/` (the app
  bundle is read-only). Keep a copy off the machine, as with `backups/eeprom/`.
- Log: `~/Library/Logs/Pakon/pakon.log`.
- Scans: one folder per order in the work folder (`~/pakon` by default, set in
  the UI), e.g. `~/pakon/000042/2026-10-11-14-30-scan.raw`. The work folder and
  the order counter are in `~/Library/Application Support/Pakon/settings.json`.
- Saved exports: in the scan's order folder (a raw opened from elsewhere gets
  a folder named after it), under `jpeg/`, `tiff/` and `raw/`.
- Temp files (the decoded roll, ~1 GB): `/tmp/pakon_web`, emptied at launch.
