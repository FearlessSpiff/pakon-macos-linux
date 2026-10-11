"""
The work folder and the lab-style order counter.

Every scan is an order: it gets the next order ID and its own folder under
the work folder, and the raw is named for the time the scan started:

    ~/pakon/000042/2026-10-11-14-30-scan.raw

The work folder (default ~/pakon) and the next order ID are kept in
settings.json under PAKON_DATA (the Mac app's ~/Library/Application
Support/Pakon), else backups/ in the repo, like the EEPROM archives. An ID is
used up when a scan starts, whether or not the scan succeeds. The counter
also skips past any order folder already in the work folder, so two installs
sharing one work folder never write into the same order.
"""
import json
import os
import threading
from datetime import datetime
from pathlib import Path

_REPO = Path(__file__).resolve().parent.parent
SETTINGS = (Path(os.environ["PAKON_DATA"]).expanduser() / "settings.json"
            if os.environ.get("PAKON_DATA") else _REPO / "backups" / "settings.json")
DEFAULT_WORK_DIR = Path.home() / "pakon"
_ID_DIGITS = 6

_lock = threading.Lock()


def _load() -> dict:
    try:
        return json.loads(SETTINGS.read_text())
    except (OSError, ValueError):
        return {}


def _save(data: dict) -> None:
    SETTINGS.parent.mkdir(parents=True, exist_ok=True)
    tmp = SETTINGS.with_suffix(".tmp")
    tmp.write_text(json.dumps(data, indent=2))
    tmp.replace(SETTINGS)


def work_dir() -> Path:
    return Path(_load().get("work_dir") or DEFAULT_WORK_DIR).expanduser()


def set_work_dir(path: Path) -> Path:
    with _lock:
        data = _load()
        data["work_dir"] = str(path)
        _save(data)
    return path


def _next_id(data: dict, base: Path) -> int:
    """The stored counter, moved past the highest order folder in `base`."""
    n = int(data.get("next_order", 1))
    try:
        taken = [int(p.name) for p in base.iterdir()
                 if p.is_dir() and p.name.isdigit()]
    except OSError:
        taken = []
    return max([n] + [t + 1 for t in taken])


def next_order() -> str:
    """The ID the next scan will get (for display; does not use it up)."""
    return f"{_next_id(_load(), work_dir()):0{_ID_DIGITS}d}"


def new_order(now: datetime | None = None) -> tuple[str, Path]:
    """Use up the next order ID: create its folder in the work folder and
    return (id, raw path). Raises OSError when the folder cannot be made."""
    now = now or datetime.now()
    with _lock:
        data = _load()
        base = Path(data.get("work_dir") or DEFAULT_WORK_DIR).expanduser()
        base.mkdir(parents=True, exist_ok=True)
        n = _next_id(data, base)
        order = f"{n:0{_ID_DIGITS}d}"
        folder = base / order
        folder.mkdir()
        data["next_order"] = n + 1
        _save(data)
    return order, folder / f"{now:%Y-%m-%d-%H-%M}-scan.raw"
