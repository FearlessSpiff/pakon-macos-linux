"""
Pakon on Linux: the web service in its own window.

Runs web.app on a free localhost port and shows it with pywebview (GTK
WebKit2 or Qt WebEngine, whichever is installed); without pywebview, or with
--browser, it opens the default browser instead and serves until Ctrl+C.
Closing the window quits. It runs from a checkout:

    python packaging/linux/pakon_linux.py [--browser]

and is what the `pakon` command of an install (install.sh, PKGBUILD) runs.

Files follow the XDG base directories, like the Mac app's ~/Library ones:

    EEPROM archives, settings   $XDG_DATA_HOME/pakon   (~/.local/share/pakon)
    decoded roll (~1 GB)        $XDG_CACHE_HOME/pakon  (~/.cache/pakon)
    pakon.log, crash.log        $XDG_STATE_HOME/pakon  (~/.local/state/pakon)

Scans go to the work folder (~/pakon unless you pick another). The scanner
needs USB access without root: install 60-pakon.rules (see README.md).
"""
import argparse
import os
import signal
import socket
import sys
import threading
import time
import webbrowser
from pathlib import Path

# The install mirrors the repo layout: web/, tools/, resources/, build/, ...
ROOT = Path(os.environ.get("PAKON_ROOT") or Path(__file__).resolve().parents[2])
sys.path.insert(0, str(ROOT))


def _xdg(var: str, fallback: str) -> Path:
    return Path(os.environ.get(var) or Path.home() / fallback) / "pakon"


DATA = _xdg("XDG_DATA_HOME", ".local/share")
CACHE = _xdg("XDG_CACHE_HOME", ".cache")
STATE = _xdg("XDG_STATE_HOME", ".local/state")
LOG = STATE / "pakon.log"


def _setup_env():
    os.environ.setdefault("PAKON_DATA", str(DATA))
    os.environ.setdefault("PAKON_WORK", str(CACHE))
    for d in (os.environ["PAKON_DATA"], os.environ["PAKON_WORK"], STATE):
        Path(d).mkdir(parents=True, exist_ok=True)
    # Started from the desktop there is no terminal; uvicorn and the tools
    # still print, so it goes to the log. From a terminal it stays there.
    if not sys.stderr or not sys.stderr.isatty():
        sys.stdout = sys.stderr = open(LOG, "a", buffering=1)


def _free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def _error_page():
    return ("<body style='font:15px sans-serif;padding:2em'>"
            "<h2>Pakon could not start its scanner service.</h2>"
            f"<p>Details are in {LOG}.</p></body>")


def main():
    ap = argparse.ArgumentParser(description="Pakon scanner app")
    ap.add_argument("--browser", action="store_true",
                    help="open the default browser instead of an app window")
    ap.add_argument("--port", type=int, default=0,
                    help="localhost port (default: a free one)")
    args = ap.parse_args()
    _setup_env()

    import uvicorn
    from web.app import app

    port = args.port or _free_port()
    url = f"http://127.0.0.1:{port}/"
    server = uvicorn.Server(uvicorn.Config(app, host="127.0.0.1", port=port,
                                           log_level="info"))
    thread = threading.Thread(target=server.run, daemon=True)
    thread.start()
    deadline = time.monotonic() + 30
    while not server.started and thread.is_alive() and time.monotonic() < deadline:
        time.sleep(0.05)

    webview = None
    if not args.browser:
        try:
            import webview
        except ImportError:
            print("pywebview is not installed: opening the browser instead",
                  file=sys.stderr)

    if webview is not None:
        webview.settings["ALLOW_DOWNLOADS"] = True    # the frame / zip links
        if server.started:
            webview.create_window("Pakon", url, width=1280, height=800,
                                  min_size=(960, 640))
        else:
            webview.create_window("Pakon", html=_error_page())
        try:
            webview.start()
        except Exception as exc:                      # no GTK / Qt backend
            print(f"no window ({exc}): opening the browser instead",
                  file=sys.stderr)
            webview = None
        else:
            server.should_exit = True                 # window closed: quit
            thread.join(timeout=5)
            return

    if not server.started:
        print(f"Pakon could not start its scanner service; see {LOG}",
              file=sys.stderr)
        sys.exit(1)
    print(f"Pakon is running at {url} (Ctrl+C to quit)", file=sys.__stderr__)
    webbrowser.open(url)
    stop = threading.Event()
    signal.signal(signal.SIGTERM, lambda *_: stop.set())
    try:
        while thread.is_alive() and not stop.wait(0.5):
            pass
    except KeyboardInterrupt:
        pass
    server.should_exit = True
    thread.join(timeout=5)


if __name__ == "__main__":
    main()
