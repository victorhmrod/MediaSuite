"""YT Downloader — download YouTube video and audio with yt-dlp.

PySide6 interface using the LocalFlix theme (colors, sidebar, cards, and Material icons).
"""

import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import urllib.request

# Windowed executable: stdout/stderr can be None, which breaks libraries that write to them.
if sys.stdout is None:
    sys.stdout = open(os.devnull, "w", encoding="utf-8")
if sys.stderr is None:
    sys.stderr = open(os.devnull, "w", encoding="utf-8")

import yt_dlp
from PySide6.QtCore import QByteArray, QObject, QPointF, QRectF, QSettings, QSize, Qt, QTimer, Signal
from PySide6.QtGui import QColor, QFont, QIcon, QImage, QLinearGradient, QPainter, QPainterPath, QPalette, QPixmap
from PySide6.QtSvg import QSvgRenderer
from PySide6.QtWidgets import (
    QApplication, QButtonGroup, QCheckBox, QComboBox, QFileDialog, QFrame, QGridLayout, QGroupBox,
    QHBoxLayout, QLabel, QLineEdit, QMainWindow, QMessageBox, QProgressBar, QPushButton, QScrollArea,
    QSizePolicy, QStackedWidget, QStyleFactory, QToolButton, QVBoxLayout, QWidget,
)

APP_NAME = "YT Downloader"
APP_VERSION = "1.0.0"
MAX_PARALLEL = 3
DEFAULT_DIR = os.path.join(os.path.expanduser("~"), "Downloads")
JS_RUNTIMES = {"deno": {}, "node": {}}


def resource_path(*parts):
    base = getattr(sys, "_MEIPASS", os.path.dirname(os.path.abspath(__file__)))
    return os.path.join(base, *parts)


def find_ffmpeg_dir():
    """Prefer FFmpeg next to the executable, then look on PATH."""
    exe_dir = os.path.dirname(sys.executable if getattr(sys, "frozen", False) else os.path.abspath(__file__))
    if os.path.isfile(os.path.join(exe_dir, "ffmpeg.exe")):
        return exe_dir
    found = shutil.which("ffmpeg")
    return os.path.dirname(found) if found else None


FFMPEG_DIR = find_ffmpeg_dir()
JS_RUNTIME = next((name for name in JS_RUNTIMES if shutil.which(name)), None)


# ---------------------------------------------------------------- theme (shared with LocalFlix)
class Colors:
    BG = "#0f1115"
    SURFACE = "#171a21"
    RAISED = "#1f232c"
    BORDER = "#2b303b"
    TEXT = "#e8eaf0"
    MUTED = "#9aa3b2"
    ACCENT = "#5b8def"
    ACCENT_HOVER = "#749ff3"
    SUCCESS = "#3ecf8e"
    WARNING = "#f5a524"
    DANGER = "#ef5350"


# Material Design icons (Apache License 2.0), viewBox 24x24.
ICONS = {
    "download": "M19 9h-4V3H9v6H5l7 7 7-7zM5 18v2h14v-2H5z",
    "list": "M3 13h2v-2H3v2zm0 4h2v-2H3v2zm0-8h2V7H3v2zm4 4h14v-2H7v2zm0 4h14v-2H7v2zM7 7v2h14V7H7z",
    "settings": "M19.14 12.94c.04-.3.06-.61.06-.94 0-.32-.02-.64-.07-.94l2.03-1.58a.49.49 0 0 0 .12-.61l-1.92-3.32a.488.488 0 0 0-.59-.22l-2.39.96c-.5-.38-1.03-.7-1.62-.94l-.36-2.54a.484.484 0 0 0-.48-.41h-3.84c-.24 0-.43.17-.47.41l-.36 2.54c-.59.24-1.13.57-1.62.94l-2.39-.96c-.22-.08-.47 0-.59.22L2.74 8.87c-.12.21-.08.47.12.61l2.03 1.58c-.05.3-.09.63-.09.94s.02.64.07.94l-2.03 1.58a.49.49 0 0 0-.12.61l1.92 3.32c.12.22.37.29.59.22l2.39-.96c.5.38 1.03.7 1.62.94l.36 2.54c.05.24.24.41.48.41h3.84c.24 0 .44-.17.47-.41l.36-2.54c.59-.24 1.13-.56 1.62-.94l2.39.96c.22.08.47 0 .59-.22l1.92-3.32c.12-.22.07-.47-.12-.61l-2.01-1.58zM12 15.6c-1.98 0-3.6-1.62-3.6-3.6s1.62-3.6 3.6-3.6 3.6 1.62 3.6 3.6-1.62 3.6-3.6 3.6z",
    "folder": "M20 6h-8l-2-2H4c-1.1 0-1.99.9-1.99 2L2 18c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2zm0 12H4V8h16v10z",
    "paste": "M19 2h-4.18C14.4.84 13.3 0 12 0c-1.3 0-2.4.84-2.82 2H5c-1.1 0-2 .9-2 2v16c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2V4c0-1.1-.9-2-2-2zm-7 0c.55 0 1 .45 1 1s-.45 1-1 1-1-.45-1-1 .45-1 1-1zm7 18H5V4h2v3h10V4h2v16z",
    "search": "M15.5 14h-.79l-.28-.27A6.471 6.471 0 0 0 16 9.5 6.5 6.5 0 1 0 9.5 16c1.61 0 3.09-.59 4.23-1.57l.27.28v.79l5 4.99L20.49 19l-4.99-5zm-6 0C7.01 14 5 11.99 5 9.5S7.01 5 9.5 5 14 7.01 14 9.5 11.99 14 9.5 14z",
    "play": "M8 5v14l11-7z",
    "close": "M19 6.41L17.59 5 12 10.59 6.41 5 5 6.41 10.59 12 5 17.59 6.41 19 12 13.41 17.59 19 19 17.59 13.41 12z",
    "refresh": "M17.65 6.35A7.958 7.958 0 0 0 12 4c-4.42 0-7.99 3.58-7.99 8s3.57 8 7.99 8c3.73 0 6.84-2.55 7.73-6h-2.08A5.99 5.99 0 0 1 12 18c-3.31 0-6-2.69-6-6s2.69-6 6-6c1.66 0 3.14.69 4.22 1.78L13 11h7V4l-2.35 2.35z",
    "delete": "M6 19c0 1.1.9 2 2 2h8c1.1 0 2-.9 2-2V7H6v12zM19 4h-3.5l-1-1h-5l-1 1H5v2h14V4z",
    "check": "M9 16.17L4.83 12l-1.42 1.41L9 19 21 7l-1.41-1.41z",
    "chevron": "M7.41 8.59L12 13.17l4.59-4.58L18 10l-6 6-6-6 1.41-1.41z",
}

STYLESHEET = """
QWidget { color: %text%; font-size: 10pt; }
QMainWindow, QDialog, QMessageBox, QStackedWidget { background: %bg%; }
QLabel { background: transparent; }
QToolTip { background: %raised%; color: %text%; border: 1px solid %border%; padding: 4px 8px; }

QFrame#Sidebar { background: %surface%; border-right: 1px solid %border%; }
QLabel#Brand { font-size: 17pt; font-weight: 700; letter-spacing: 0.5px; }
QLabel#PageTitle { font-size: 20pt; font-weight: 700; }
QLabel#SectionTitle { font-size: 13pt; font-weight: 600; }
QLabel#CardTitle { font-size: 11pt; font-weight: 600; }
QLabel#Muted, QLabel[muted="true"] { color: %muted%; }
QLabel#Empty { color: %muted%; font-size: 11pt; }

QPushButton, QToolButton {
    background: %raised%; border: 1px solid %border%; border-radius: 8px; padding: 7px 16px;
}
QPushButton:hover, QToolButton:hover { background: #272c37; border-color: #3a4150; }
QPushButton:pressed, QToolButton:pressed { background: #1a1e26; }
QPushButton:disabled, QToolButton:disabled { color: #5d6573; background: %surface%; border-color: %border%; }
QPushButton:focus { border-color: %accent%; }
QPushButton[variant="primary"] { background: %accent%; border-color: %accent%; color: #ffffff; font-weight: 600; }
QPushButton[variant="primary"]:hover { background: %accentHover%; border-color: %accentHover%; }
QPushButton[variant="primary"]:disabled { background: #2a3a5c; border-color: #2a3a5c; color: #7e8aa3; }
QPushButton[variant="ghost"], QToolButton[variant="ghost"] { background: transparent; border-color: transparent; }
QPushButton[variant="ghost"]:hover, QToolButton[variant="ghost"]:hover { background: rgba(255,255,255,0.08); }
QToolButton[nav="true"] {
    background: transparent; border: none; border-radius: 10px; padding: 10px 14px; text-align: left; font-size: 11pt;
}
QToolButton[nav="true"]:hover { background: rgba(255,255,255,0.06); }
QToolButton[nav="true"]:checked { background: rgba(91,141,239,0.18); color: %accentHover%; font-weight: 600; }
QToolButton[iconButton="true"] { background: transparent; border: none; border-radius: 18px; padding: 6px; }
QToolButton[iconButton="true"]:hover { background: rgba(255,255,255,0.12); }
QToolButton[iconButton="true"]:pressed { background: rgba(255,255,255,0.20); }

QLineEdit, QComboBox {
    background: %surface%; border: 1px solid %border%; border-radius: 8px; padding: 7px 10px;
    selection-background-color: %accent%; selection-color: #ffffff;
}
QLineEdit[size="large"] { padding: 10px 14px; font-size: 11pt; }
QLineEdit:focus, QComboBox:focus { border-color: %accent%; }
QComboBox::drop-down { border: none; width: 24px; }
QComboBox::down-arrow { image: url(%chevron%); width: 14px; height: 14px; }
QComboBox:disabled { color: #5d6573; }
QComboBox QAbstractItemView { background: %raised%; border: 1px solid %border%; selection-background-color: %accent%; outline: none; padding: 4px; }

QCheckBox { spacing: 10px; background: transparent; }
QCheckBox::indicator { width: 18px; height: 18px; border-radius: 5px; border: 1px solid #4a5160; background: %surface%; }
QCheckBox::indicator:hover { border-color: %accent%; }
QCheckBox::indicator:checked { background: %accent%; border-color: %accent%; image: url(%check%); }

QProgressBar { background: #2a2f3a; border: none; border-radius: 3px; max-height: 6px; min-height: 6px; text-align: center; }
QProgressBar::chunk { background: %accent%; border-radius: 3px; }
QProgressBar[state="done"]::chunk { background: %success%; }
QProgressBar[state="error"]::chunk { background: %danger%; }

QScrollArea { border: none; background: transparent; }
QScrollArea > QWidget > QWidget { background: transparent; }
QScrollBar:vertical { background: transparent; width: 12px; margin: 2px; }
QScrollBar::handle:vertical { background: #353b48; border-radius: 4px; min-height: 36px; }
QScrollBar::handle:vertical:hover { background: #4a5263; }
QScrollBar::add-line, QScrollBar::sub-line, QScrollBar::add-page, QScrollBar::sub-page { background: none; border: none; width: 0; height: 0; }

QGroupBox { border: 1px solid %border%; border-radius: 12px; margin-top: 14px; padding: 18px 14px 12px 14px; background: %surface%; }
QGroupBox::title { subcontrol-origin: margin; left: 14px; padding: 0 6px; color: %muted%; font-weight: 600; }

QFrame#Card { background: %surface%; border: 1px solid %border%; border-radius: 14px; }
QLabel#Badge { background: rgba(91,141,239,0.18); color: %accentHover%; border-radius: 9px; padding: 1px 8px; font-size: 9pt; font-weight: 600; }
"""


def svg_icon_file(name, color):
    """Write an SVG icon to disk for stylesheet url() references."""
    folder = os.path.join(tempfile.gettempdir(), "ytdownloader-ui")
    os.makedirs(folder, exist_ok=True)
    path = os.path.join(folder, f"{name}.svg")
    with open(path, "w", encoding="utf-8") as f:
        f.write(f"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'><path fill='{color}' d='{ICONS[name]}'/></svg>")
    return path.replace("\\", "/")


def stylesheet():
    css = STYLESHEET
    for key, value in {
        "%bg%": Colors.BG, "%surface%": Colors.SURFACE, "%raised%": Colors.RAISED, "%border%": Colors.BORDER,
        "%text%": Colors.TEXT, "%muted%": Colors.MUTED, "%accentHover%": Colors.ACCENT_HOVER,
        "%accent%": Colors.ACCENT, "%success%": Colors.SUCCESS, "%danger%": Colors.DANGER,
        "%chevron%": svg_icon_file("chevron", Colors.MUTED), "%check%": svg_icon_file("check", "#ffffff"),
    }.items():
        css = css.replace(key, value)
    return css


_icon_cache = {}


def icon(name, color=Colors.TEXT, size=24):
    key = (name, color, size)
    if key in _icon_cache:
        return _icon_cache[key]
    svg = f"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'><path fill='{color}' d='{ICONS[name]}'/></svg>"
    renderer = QSvgRenderer(QByteArray(svg.encode()))
    out = QIcon()
    for dpr in (1, 2):
        px = size * dpr
        pm = QPixmap(px, px)
        pm.fill(Qt.transparent)
        painter = QPainter(pm)
        painter.setRenderHint(QPainter.Antialiasing)
        renderer.render(painter, QRectF(0, 0, px, px))
        painter.end()
        pm.setDevicePixelRatio(dpr)
        out.addPixmap(pm)
    _icon_cache[key] = out
    return out


def apply_theme(app):
    app.setStyle(QStyleFactory.create("Fusion"))
    font = app.font()
    font.setFamily("Segoe UI")
    font.setPointSize(10)
    app.setFont(font)

    p = QPalette()
    p.setColor(QPalette.Window, QColor(Colors.BG))
    p.setColor(QPalette.WindowText, QColor(Colors.TEXT))
    p.setColor(QPalette.Base, QColor(Colors.SURFACE))
    p.setColor(QPalette.AlternateBase, QColor("#1a1d25"))
    p.setColor(QPalette.Text, QColor(Colors.TEXT))
    p.setColor(QPalette.Button, QColor(Colors.RAISED))
    p.setColor(QPalette.ButtonText, QColor(Colors.TEXT))
    p.setColor(QPalette.ToolTipBase, QColor(Colors.RAISED))
    p.setColor(QPalette.ToolTipText, QColor(Colors.TEXT))
    p.setColor(QPalette.Highlight, QColor(Colors.ACCENT))
    p.setColor(QPalette.HighlightedText, Qt.white)
    p.setColor(QPalette.PlaceholderText, QColor(Colors.MUTED))
    p.setColor(QPalette.Link, QColor(Colors.ACCENT_HOVER))
    for role in (QPalette.Text, QPalette.ButtonText, QPalette.WindowText):
        p.setColor(QPalette.Disabled, role, QColor("#626a78"))
    app.setPalette(p)
    app.setStyleSheet(stylesheet())


def use_dark_title_bar(widget):
    if sys.platform != "win32":
        return
    import ctypes
    hwnd = int(widget.winId())
    on = ctypes.c_int(1)
    # 20 = DWMWA_USE_IMMERSIVE_DARK_MODE (19 on early Windows 10 builds)
    if ctypes.windll.dwmapi.DwmSetWindowAttribute(hwnd, 20, ctypes.byref(on), ctypes.sizeof(on)) != 0:
        ctypes.windll.dwmapi.DwmSetWindowAttribute(hwnd, 19, ctypes.byref(on), ctypes.sizeof(on))


# ---------------------------------------------------------------- utilities
ANSI_RE = re.compile(r"\x1b\[[0-9;]*m")


def clean_error(msg):
    msg = ANSI_RE.sub("", str(msg)).strip()
    return re.sub(r"^ERROR:\s*", "", msg)


def fmt_bytes(n):
    if not n:
        return ""
    for unit in ("B", "KB", "MB", "GB"):
        if n < 1024:
            return f"{n:.1f} {unit}" if unit != "B" else f"{n:.0f} B"
        n /= 1024
    return f"{n:.1f} TB"


def fmt_duration(seconds):
    if not seconds:
        return ""
    seconds = int(seconds)
    h, rem = divmod(seconds, 3600)
    m, s = divmod(rem, 60)
    return f"{h}:{m:02d}:{s:02d}" if h else f"{m}:{s:02d}"


def fmt_count(n):
    if not n:
        return ""
    if n >= 1_000_000_000:
        return f"{n / 1e9:.1f} B"
    if n >= 1_000_000:
        return f"{n / 1e6:.1f} M"
    if n >= 1_000:
        return f"{n / 1e3:.0f} K"
    return str(n)


def thumbnail_url(info):
    vid = info.get("id")
    if info.get("extractor_key", "").startswith("Youtube") and vid and info.get("_type") != "playlist":
        return f"https://i.ytimg.com/vi/{vid}/hqdefault.jpg"
    thumbs = [t for t in info.get("thumbnails") or [] if t.get("url")]
    jpgs = [t for t in thumbs if ".jpg" in t["url"]]
    if jpgs or thumbs:
        return (jpgs or thumbs)[-1]["url"]
    return info.get("thumbnail")


def open_path(path, select=False):
    if not path or not os.path.exists(path):
        return
    if sys.platform == "win32":
        if select:
            subprocess.Popen(["explorer", "/select,", os.path.normpath(path)])
        else:
            os.startfile(path)
    else:
        subprocess.Popen(["xdg-open", path])


class _AsyncCall(QObject):
    done = Signal(object)


_pending_calls = set()


def run_async(fn, on_done):
    """Run fn() on a worker and deliver (ok, result|error) to on_done on the UI thread."""
    call = _AsyncCall()
    _pending_calls.add(call)

    def deliver(result):
        _pending_calls.discard(call)
        on_done(*result)

    call.done.connect(deliver)

    def target():
        try:
            call.done.emit((True, fn()))
        except Exception as e:  # noqa: BLE001 - report any failure as a message in the interface
            call.done.emit((False, e))

    threading.Thread(target=target, daemon=True).start()


def fetch_bytes(url):
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    with urllib.request.urlopen(req, timeout=15) as r:
        return r.read()


class _SilentLogger:
    def debug(self, msg): pass
    def info(self, msg): pass
    def warning(self, msg): pass
    def error(self, msg): pass


def base_opts():
    opts = {"quiet": True, "no_warnings": True, "noprogress": True, "js_runtimes": JS_RUNTIMES,
            "logger": _SilentLogger()}
    if FFMPEG_DIR:
        opts["ffmpeg_location"] = FFMPEG_DIR
    return opts


def fetch_info(url):
    opts = base_opts() | {"skip_download": True, "noplaylist": True, "extract_flat": "in_playlist"}
    with yt_dlp.YoutubeDL(opts) as ydl:
        return ydl.sanitize_info(ydl.extract_info(url, download=False))


def format_for(audio, height):
    if audio:
        return "bestaudio/best"
    if not FFMPEG_DIR:
        return f"best[ext=mp4][height<={height}]/best[height<={height}]/best" if height else "best[ext=mp4]/best"
    if height:
        return (f"bestvideo[height<={height}][ext=mp4]+bestaudio[ext=m4a]/"
                f"bestvideo[height<={height}]+bestaudio/best[height<={height}]/best")
    return "bestvideo[ext=mp4]+bestaudio[ext=m4a]/bestvideo+bestaudio/best"


# ---------------------------------------------------------------- widgets
class Thumb(QWidget):
    """Rounded thumbnail; without an image, show a gradient with initials, like LocalFlix."""

    def __init__(self, width, height, radius=10, parent=None):
        super().__init__(parent)
        self.setFixedSize(width, height)
        self._radius = radius
        self._image = None
        self._title = ""

    def set_title(self, title):
        self._title = title or ""
        self.update()

    def set_image_data(self, data):
        img = QImage()
        self._image = img if img.loadFromData(data) else None
        self.update()

    def load(self, url):
        self._image = None
        self.update()
        if url:
            run_async(lambda: fetch_bytes(url), lambda ok, data: ok and self.set_image_data(data))

    def _initials(self):
        words = [w for w in self._title.split() if any(c.isalnum() for c in w)]
        out = "".join(next(c for c in w if c.isalnum()).upper() for w in words[:2])
        return out or "?"

    def paintEvent(self, _event):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        p.setRenderHint(QPainter.SmoothPixmapTransform)
        rect = QRectF(self.rect())
        path = QPainterPath()
        path.addRoundedRect(rect, self._radius, self._radius)
        p.setClipPath(path)
        if self._image is not None:
            dpr = self.devicePixelRatioF()
            target = QSize(int(rect.width() * dpr), int(rect.height() * dpr))
            scaled = self._image.scaled(target, Qt.KeepAspectRatioByExpanding, Qt.SmoothTransformation)
            scaled.setDevicePixelRatio(dpr)
            offset = QPointF((rect.width() - scaled.width() / dpr) / 2, (rect.height() - scaled.height() / dpr) / 2)
            p.drawImage(offset, scaled)
        else:
            h = hashlib.md5(self._title.encode()).digest()
            hue = h[0] * 360 // 256
            g = QLinearGradient(0, 0, rect.width(), rect.height())
            g.setColorAt(0, QColor.fromHsl(hue, 90, 70).darker(230))
            g.setColorAt(1, QColor.fromHsl((hue + 40) % 360, 90, 70).darker(330))
            p.fillRect(rect, g)
            f = QFont("Segoe UI")
            f.setBold(True)
            f.setPixelSize(int(rect.height() * 0.28))
            p.setFont(f)
            p.setPen(QColor(255, 255, 255, 200))
            p.drawText(rect, Qt.AlignCenter, self._initials())
        p.end()


class ElidedLabel(QLabel):
    def __init__(self, text="", parent=None):
        super().__init__(parent)
        self.setSizePolicy(QSizePolicy.Ignored, QSizePolicy.Preferred)
        self.setMinimumWidth(20)
        self._full = ""
        self.setText(text)

    def setText(self, text):
        self._full = text or ""
        self.setToolTip(self._full)
        self._refresh()

    def resizeEvent(self, event):
        super().resizeEvent(event)
        self._refresh()

    def _refresh(self):
        super().setText(self.fontMetrics().elidedText(self._full, Qt.ElideRight, max(self.width(), 20)))


def label(text="", name=None, muted=False, wrap=False):
    w = QLabel(text)
    if name:
        w.setObjectName(name)
    if muted:
        w.setProperty("muted", True)
    w.setWordWrap(wrap)
    return w


def button(text, icon_name=None, variant=None, icon_color=Colors.TEXT):
    b = QPushButton(text)
    if icon_name:
        b.setIcon(icon(icon_name, "#ffffff" if variant == "primary" else icon_color, 18))
        b.setIconSize(QSize(18, 18))
    if variant:
        b.setProperty("variant", variant)
    b.setCursor(Qt.PointingHandCursor)
    return b


def icon_button(icon_name, tooltip, color=Colors.TEXT):
    b = QToolButton()
    b.setProperty("iconButton", True)
    b.setIcon(icon(icon_name, color, 20))
    b.setIconSize(QSize(20, 20))
    b.setToolTip(tooltip)
    b.setCursor(Qt.PointingHandCursor)
    return b


def repolish(w):
    w.style().unpolish(w)
    w.style().polish(w)


# ---------------------------------------------------------------- downloads
class Cancelled(Exception):
    pass


_slots = threading.Semaphore(MAX_PARALLEL)


class DownloadJob(QObject):
    progress = Signal(float, str)  # percentage (-1 = indeterminate), status text
    state_changed = Signal(str, str)  # queued | running | done | error | cancelled, message

    def __init__(self, url, title, thumb, out_dir, audio, height, playlist, quality_label):
        super().__init__()
        self.url, self.title, self.thumb = url, title, thumb
        self.out_dir, self.audio, self.height, self.playlist = out_dir, audio, height, playlist
        self.quality_label = quality_label
        self.state = "queued"
        self.result_path = None
        self._cancel = threading.Event()

    @property
    def active(self):
        return self.state in ("queued", "running")

    def start(self):
        self._cancel.clear()
        self._set_state("queued", "Queued...")
        threading.Thread(target=self._run, daemon=True).start()

    def cancel(self):
        self._cancel.set()

    def _set_state(self, state, msg):
        self.state = state
        self.state_changed.emit(state, msg)

    def _hook(self, d):
        if self._cancel.is_set():
            raise Cancelled()
        info = d.get("info_dict") or {}
        prefix = ""
        if self.playlist and info.get("playlist_index"):
            total = info.get("n_entries") or info.get("playlist_count")
            prefix = f"Video {info['playlist_index']}" + (f" of {total}" if total else "") + " · "
        if d["status"] == "downloading":
            total = d.get("total_bytes") or d.get("total_bytes_estimate")
            done = d.get("downloaded_bytes") or 0
            pct = done / total * 100 if total else -1
            kind = "audio" if info.get("vcodec") == "none" and not self.audio else ""
            parts = [f"Downloading {kind}".strip()]
            if total:
                parts.append(f"{pct:.0f}%  ·  {fmt_bytes(done)} of {fmt_bytes(total)}")
            if d.get("speed"):
                parts.append(f"{fmt_bytes(d['speed'])}/s")
            if d.get("eta") is not None:
                parts.append(f"time left {fmt_duration(d['eta']) or '0:00'}")
            self.progress.emit(pct, prefix + "  ·  ".join(parts))
        elif d["status"] == "finished":
            self.progress.emit(100, prefix + "Download finished, processing...")

    def _pp_hook(self, d):
        if self._cancel.is_set():
            raise Cancelled()
        if d["status"] == "started":
            pp = d.get("postprocessor", "")
            if pp == "Merger":
                self.progress.emit(-1, "Merging video and audio...")
            elif pp == "ExtractAudio":
                self.progress.emit(-1, "Converting to MP3...")

    def _run(self):
        with _slots:
            if self._cancel.is_set():
                self._set_state("cancelled", "Cancelled.")
                return
            self._set_state("running", "Connecting...")
            if self.playlist:
                tmpl = os.path.join(self.out_dir, "%(playlist_title)s", "%(playlist_index)03d - %(title)s.%(ext)s")
            else:
                tmpl = os.path.join(self.out_dir, "%(title)s.%(ext)s")
            opts = base_opts() | {
                "outtmpl": tmpl,
                "format": format_for(self.audio, self.height),
                "noplaylist": not self.playlist,
                "ignoreerrors": self.playlist,
                "windowsfilenames": True,
                "progress_hooks": [self._hook],
                "postprocessor_hooks": [self._pp_hook],
            }
            if self.audio:
                opts["postprocessors"] = [{"key": "FFmpegExtractAudio", "preferredcodec": "mp3", "preferredquality": "192"}]
            elif FFMPEG_DIR:
                opts["merge_output_format"] = "mp4"
            try:
                with yt_dlp.YoutubeDL(opts) as ydl:
                    info = ydl.extract_info(self.url, download=True)
                if self.playlist:
                    title = (info or {}).get("title") or ""
                    self.result_path = os.path.join(self.out_dir, yt_dlp.utils.sanitize_filename(title, restricted=False)) \
                        if title else self.out_dir
                    if not os.path.isdir(self.result_path):
                        self.result_path = self.out_dir
                else:
                    downloads = (info or {}).get("requested_downloads") or [{}]
                    self.result_path = downloads[0].get("filepath")
                self._set_state("done", "Finished")
            except Cancelled:
                self._set_state("cancelled", "Cancelled.")
            except yt_dlp.utils.DownloadError as e:
                if self._cancel.is_set():
                    self._set_state("cancelled", "Cancelled.")
                else:
                    self._set_state("error", clean_error(e))
            except Exception as e:  # noqa: BLE001
                self._set_state("error", f"Unexpected error: {clean_error(e)}")


class DownloadCard(QFrame):
    removed = Signal(object)

    def __init__(self, job):
        super().__init__()
        self.job = job
        self.setObjectName("Card")
        lay = QHBoxLayout(self)
        lay.setContentsMargins(14, 14, 14, 14)
        lay.setSpacing(16)

        self.thumb = Thumb(192, 108, 8)
        self.thumb.set_title(job.title)
        self.thumb.load(job.thumb)
        lay.addWidget(self.thumb)

        mid = QVBoxLayout()
        mid.setSpacing(6)
        self.title = ElidedLabel(job.title)
        self.title.setObjectName("CardTitle")
        kind = "MP3" if job.audio else f"MP4 · {job.quality_label}"
        if job.playlist:
            kind = f"Playlist · {kind}"
        self.meta = ElidedLabel(f"{kind}  ·  {job.out_dir}")
        self.meta.setProperty("muted", True)
        self.bar = QProgressBar()
        self.bar.setRange(0, 100)
        self.bar.setTextVisible(False)
        self.status = ElidedLabel("Queued...")
        self.status.setProperty("muted", True)
        mid.addWidget(self.title)
        mid.addWidget(self.meta)
        mid.addStretch(1)
        mid.addWidget(self.bar)
        mid.addWidget(self.status)
        lay.addLayout(mid, 1)

        actions = QHBoxLayout()
        actions.setSpacing(2)
        self.btn_play = icon_button("play", "Open file")
        self.btn_folder = icon_button("folder", "Show in folder")
        self.btn_retry = icon_button("refresh", "Retry")
        self.btn_cancel = icon_button("close", "Cancel")
        self.btn_remove = icon_button("delete", "Remove from list", Colors.MUTED)
        for b in (self.btn_play, self.btn_folder, self.btn_retry, self.btn_cancel, self.btn_remove):
            actions.addWidget(b)
        lay.addLayout(actions)

        self.btn_play.clicked.connect(lambda: open_path(job.result_path))
        self.btn_folder.clicked.connect(self._show_in_folder)
        self.btn_retry.clicked.connect(job.start)
        self.btn_cancel.clicked.connect(self._cancel)
        self.btn_remove.clicked.connect(lambda: self.removed.emit(self))
        job.progress.connect(self._on_progress)
        job.state_changed.connect(self._on_state)
        self._on_state("queued", "Queued...")

    def _cancel(self):
        self.job.cancel()
        self.status.setText("Cancelando...")

    def _show_in_folder(self):
        path = self.job.result_path
        if path and os.path.isfile(path):
            open_path(path, select=True)
        else:
            open_path(path or self.job.out_dir)

    def _on_progress(self, pct, text):
        if pct < 0:
            self.bar.setRange(0, 0)
        else:
            self.bar.setRange(0, 100)
            self.bar.setValue(int(pct))
        self.status.setText(text)

    def _on_state(self, state, msg):
        self.bar.setProperty("state", state if state in ("done", "error") else "")
        repolish(self.bar)
        if state in ("done", "error", "cancelled"):
            self.bar.setRange(0, 100)
            self.bar.setValue(100 if state == "done" else self.bar.value())
        if state == "queued":
            self.bar.setRange(0, 100)
            self.bar.setValue(0)
        self.status.setText(msg)
        color = {"done": Colors.SUCCESS, "error": Colors.DANGER}.get(state)
        self.status.setStyleSheet(f"color: {color};" if color else "")
        self.btn_cancel.setVisible(self.job.active)
        self.btn_remove.setVisible(not self.job.active)
        self.btn_retry.setVisible(state in ("error", "cancelled"))
        self.btn_play.setVisible(state == "done" and bool(self.job.result_path) and os.path.isfile(self.job.result_path or ""))
        self.btn_folder.setVisible(state == "done")


# ---------------------------------------------------------------- pages
def page_scaffold(title):
    page = QWidget()
    outer = QVBoxLayout(page)
    outer.setContentsMargins(32, 28, 32, 24)
    outer.setSpacing(16)
    header = QHBoxLayout()
    header.addWidget(label(title, "PageTitle"))
    header.addStretch(1)
    outer.addLayout(header)
    return page, outer, header


class DownloadPage(QWidget):
    download_requested = Signal(object)

    def __init__(self, settings):
        super().__init__()
        self.settings = settings
        self.info = None
        self.url = ""
        page, outer, _ = page_scaffold("Download")
        QVBoxLayout(self).addWidget(page)
        self.layout().setContentsMargins(0, 0, 0, 0)

        outer.addWidget(label("Paste a YouTube video or playlist link.", muted=True))

        row = QHBoxLayout()
        row.setSpacing(8)
        self.url_edit = QLineEdit()
        self.url_edit.setProperty("size", "large")
        self.url_edit.setPlaceholderText("https://www.youtube.com/watch?v=...")
        self.url_edit.setClearButtonEnabled(True)
        self.url_edit.returnPressed.connect(self.search)
        paste = button("Paste", "paste")
        paste.clicked.connect(self._paste)
        self.search_btn = button("Look up", "search", "primary")
        self.search_btn.clicked.connect(self.search)
        row.addWidget(self.url_edit, 1)
        row.addWidget(paste)
        row.addWidget(self.search_btn)
        outer.addLayout(row)

        self.message = label("", "Muted", wrap=True)
        self.message.hide()
        outer.addWidget(self.message)

        self.card = self._build_card()
        self.card.hide()
        outer.addWidget(self.card)

        self.empty = label("No video loaded yet.\nPaste a link above and click Look up.", "Empty")
        self.empty.setAlignment(Qt.AlignCenter)
        outer.addWidget(self.empty, 1)
        outer.addStretch(0)

    def _build_card(self):
        card = QFrame()
        card.setObjectName("Card")
        lay = QHBoxLayout(card)
        lay.setContentsMargins(18, 18, 18, 18)
        lay.setSpacing(20)
        self.thumb = Thumb(384, 216, 10)
        lay.addWidget(self.thumb, 0, Qt.AlignTop)

        right = QVBoxLayout()
        right.setSpacing(8)
        self.badge = label("PLAYLIST", "Badge")
        self.title = label("", "SectionTitle", wrap=True)
        self.meta = label("", "Muted", wrap=True)
        badge_row = QHBoxLayout()
        badge_row.addWidget(self.badge)
        badge_row.addStretch(1)
        right.addLayout(badge_row)
        right.addWidget(self.title)
        right.addWidget(self.meta)
        right.addSpacing(8)

        grid = QGridLayout()
        grid.setHorizontalSpacing(12)
        grid.setVerticalSpacing(10)
        self.format_box = QComboBox()
        self.format_box.addItem("Video (MP4)", "video")
        self.format_box.addItem("Audio (MP3)", "audio")
        self.format_box.currentIndexChanged.connect(self._on_format)
        self.quality_box = QComboBox()
        self.quality_box.setMinimumWidth(170)
        grid.addWidget(label("Format", muted=True), 0, 0)
        grid.addWidget(self.format_box, 0, 1)
        grid.addWidget(label("Quality", muted=True), 1, 0)
        grid.addWidget(self.quality_box, 1, 1)
        grid.addWidget(label("Save to", muted=True), 2, 0)
        self.dir_label = ElidedLabel()
        change = button("Alterar", "folder", "ghost")
        change.clicked.connect(self._choose_dir)
        dir_row = QHBoxLayout()
        dir_row.addWidget(self.dir_label, 1)
        dir_row.addWidget(change)
        grid.addLayout(dir_row, 2, 1)
        grid.setColumnStretch(1, 1)
        right.addLayout(grid)

        self.playlist_check = QCheckBox("Download the entire playlist")
        right.addWidget(self.playlist_check)
        right.addStretch(1)

        self.download_btn = button("Download", "download", "primary")
        self.download_btn.setMinimumHeight(40)
        self.download_btn.clicked.connect(self._download)
        btn_row = QHBoxLayout()
        btn_row.addWidget(self.download_btn)
        btn_row.addStretch(1)
        right.addLayout(btn_row)
        lay.addLayout(right, 1)
        return card

    # -- actions
    def _paste(self):
        text = QApplication.clipboard().text().strip()
        if text:
            self.url_edit.setText(text)
            self.search()

    def _choose_dir(self):
        d = QFileDialog.getExistingDirectory(self, "Choose folder", self.out_dir)
        if d:
            self.out_dir = d
            self.dir_label.setText(d)

    def _on_format(self):
        self.quality_box.setEnabled(self.format_box.currentData() == "video")

    def _show_message(self, text, error=False):
        self.message.setText(text)
        self.message.setStyleSheet(f"color: {Colors.DANGER};" if error else "")
        self.message.setVisible(bool(text))

    def search(self):
        url = self.url_edit.text().strip()
        if not url:
            self.url_edit.setFocus()
            return
        if not re.match(r"^https?://", url):
            url = "https://" + url
        self.url = url
        self.search_btn.setEnabled(False)
        self.search_btn.setText("Looking up...")
        self._show_message("Looking up video information...")
        run_async(lambda: fetch_info(url), self._on_info)

    def _on_info(self, ok, result):
        self.search_btn.setEnabled(True)
        self.search_btn.setText("Look up")
        if not ok:
            self.card.hide()
            self.empty.show()
            self._show_message(f"Could not load the video: {clean_error(result)}", error=True)
            return
        self._show_message("")
        self.info = result
        self._fill_card(result)

    def _fill_card(self, info):
        is_playlist = info.get("_type") == "playlist"
        title = info.get("title") or "Untitled"
        self.title.setText(title)
        self.thumb.set_title(title)
        if is_playlist:
            count = info.get("playlist_count") or len(info.get("entries") or [])
            meta = [info.get("uploader") or info.get("channel") or "", f"{count} videos"]
            entries = info.get("entries") or []
            thumb = thumbnail_url(entries[0]) if entries else None
            self.thumb.load(thumb or thumbnail_url(info))
        else:
            views = fmt_count(info.get("view_count"))
            meta = [info.get("channel") or info.get("uploader") or "", fmt_duration(info.get("duration")),
                    f"{views} views" if views else ""]
            self.thumb.load(thumbnail_url(info))
        self.meta.setText("  ·  ".join(m for m in meta if m))
        self.badge.setVisible(is_playlist)

        has_list = "list=" in self.url
        self.playlist_check.setVisible(has_list or is_playlist)
        self.playlist_check.setChecked(is_playlist)
        self.playlist_check.setEnabled(not is_playlist)

        heights = sorted({f["height"] for f in info.get("formats") or []
                          if f.get("height") and f.get("vcodec") not in (None, "none")}, reverse=True)
        if not heights:
            heights = [2160, 1440, 1080, 720, 480, 360]
        self.quality_box.clear()
        self.quality_box.addItem("Best quality", 0)
        for h in heights:
            self.quality_box.addItem(f"{h}p", h)
        preferred = int(self.settings.value("quality", 0))
        idx = self.quality_box.findData(preferred) if preferred else 0
        if idx < 0:  # preferred quality unavailable: choose the highest available below it
            idx = next((i for i in range(1, self.quality_box.count()) if self.quality_box.itemData(i) <= preferred), 0)
        self.quality_box.setCurrentIndex(idx)
        self.format_box.setCurrentIndex(self.format_box.findData(self.settings.value("format", "video")))
        self._on_format()

        self.out_dir = self.settings.value("out_dir", DEFAULT_DIR)
        self.dir_label.setText(self.out_dir)
        self.empty.hide()
        self.card.show()

    def _download(self):
        if not self.info:
            return
        audio = self.format_box.currentData() == "audio"
        if audio and not FFMPEG_DIR:
            QMessageBox.warning(self, APP_NAME, "MP3 conversion requires FFmpeg to be installed "
                                                "(or ffmpeg.exe next to the application).")
            return
        try:
            os.makedirs(self.out_dir, exist_ok=True)
        except OSError as e:
            QMessageBox.critical(self, APP_NAME, f"Could not use the folder:\n{e}")
            return
        playlist = self.playlist_check.isVisible() and self.playlist_check.isChecked()
        url = self.url
        if not playlist and self.info.get("_type") != "playlist" and self.info.get("webpage_url"):
            url = self.info["webpage_url"]
        job = DownloadJob(
            url=url, title=self.info.get("title") or url, thumb=self.thumb_url_for_job(),
            out_dir=self.out_dir, audio=audio, height=self.quality_box.currentData() or 0,
            playlist=playlist, quality_label=self.quality_box.currentText(),
        )
        self.download_requested.emit(job)

    def thumb_url_for_job(self):
        info = self.info or {}
        if info.get("_type") == "playlist":
            entries = info.get("entries") or []
            return thumbnail_url(entries[0]) if entries else thumbnail_url(info)
        return thumbnail_url(info)


class DownloadsPage(QWidget):
    active_changed = Signal(int)

    def __init__(self):
        super().__init__()
        self.cards = []
        page, outer, header = page_scaffold("Downloads")
        QVBoxLayout(self).addWidget(page)
        self.layout().setContentsMargins(0, 0, 0, 0)

        clear = button("Clear finished", "delete", "ghost", Colors.MUTED)
        clear.clicked.connect(self.clear_finished)
        header.addWidget(clear)

        self.scroll = QScrollArea()
        self.scroll.setWidgetResizable(True)
        self.scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        holder = QWidget()
        self.list = QVBoxLayout(holder)
        self.list.setContentsMargins(0, 0, 8, 0)
        self.list.setSpacing(12)
        self.empty = label("No downloads yet.\nLook up a video on the Download page.", "Empty")
        self.empty.setAlignment(Qt.AlignCenter)
        self.empty.setMinimumHeight(240)
        self.list.addWidget(self.empty)
        self.list.addStretch(1)
        self.scroll.setWidget(holder)
        outer.addWidget(self.scroll, 1)

    def add(self, job):
        card = DownloadCard(job)
        card.removed.connect(self.remove)
        job.state_changed.connect(lambda *_: self._emit_active())
        self.cards.insert(0, card)
        self.list.insertWidget(0, card)
        self.empty.hide()
        job.start()
        self._emit_active()

    def remove(self, card):
        if card.job.active:
            return
        self.cards.remove(card)
        card.deleteLater()
        self.empty.setVisible(not self.cards)

    def clear_finished(self):
        for card in [c for c in self.cards if c.job.state == "done"]:
            self.remove(card)

    def active_count(self):
        return sum(1 for c in self.cards if c.job.active)

    def cancel_all(self):
        for c in self.cards:
            c.job.cancel()

    def _emit_active(self):
        self.active_changed.emit(self.active_count())


class SettingsPage(QWidget):
    def __init__(self, settings):
        super().__init__()
        self.settings = settings
        page, outer, _ = page_scaffold("Settings")
        QVBoxLayout(self).addWidget(page)
        self.layout().setContentsMargins(0, 0, 0, 0)

        group = QGroupBox("Downloads")
        grid = QGridLayout(group)
        grid.setHorizontalSpacing(14)
        grid.setVerticalSpacing(12)
        self.dir_edit = QLineEdit(settings.value("out_dir", DEFAULT_DIR))
        self.dir_edit.editingFinished.connect(lambda: settings.setValue("out_dir", self.dir_edit.text().strip()))
        browse = button("Browse...", "folder")
        browse.clicked.connect(self._browse)
        grid.addWidget(label("Default folder", muted=True), 0, 0)
        grid.addWidget(self.dir_edit, 0, 1)
        grid.addWidget(browse, 0, 2)

        self.format_box = QComboBox()
        self.format_box.addItem("Video (MP4)", "video")
        self.format_box.addItem("Audio (MP3)", "audio")
        self.format_box.setCurrentIndex(self.format_box.findData(settings.value("format", "video")))
        self.format_box.currentIndexChanged.connect(lambda: settings.setValue("format", self.format_box.currentData()))
        grid.addWidget(label("Default format", muted=True), 1, 0)
        grid.addWidget(self.format_box, 1, 1)

        self.quality_box = QComboBox()
        self.quality_box.addItem("Best quality", 0)
        for h in (2160, 1440, 1080, 720, 480, 360):
            self.quality_box.addItem(f"{h}p", h)
        self.quality_box.setCurrentIndex(max(0, self.quality_box.findData(int(settings.value("quality", 0)))))
        self.quality_box.currentIndexChanged.connect(lambda: settings.setValue("quality", self.quality_box.currentData()))
        grid.addWidget(label("Default quality", muted=True), 2, 0)
        grid.addWidget(self.quality_box, 2, 1)
        grid.setColumnStretch(1, 1)
        outer.addWidget(group)

        about = QGroupBox("Components")
        av = QGridLayout(about)
        av.setHorizontalSpacing(14)
        av.setVerticalSpacing(10)
        rows = [
            ("yt-dlp", yt_dlp.version.__version__, True),
            ("ffmpeg", FFMPEG_DIR or "not found — MP3 and high-quality video unavailable", bool(FFMPEG_DIR)),
            ("JavaScript runtime", JS_RUNTIME or "not found — install Node.js or Deno", bool(JS_RUNTIME)),
        ]
        for i, (name, value, ok) in enumerate(rows):
            dot = QLabel()
            dot.setPixmap(icon("check" if ok else "close", Colors.SUCCESS if ok else Colors.WARNING, 16).pixmap(16, 16))
            av.addWidget(dot, i, 0)
            av.addWidget(label(name), i, 1)
            value_label = ElidedLabel(value)
            value_label.setProperty("muted", True)
            av.addWidget(value_label, i, 2)
        av.setColumnStretch(2, 1)
        outer.addWidget(about)
        outer.addStretch(1)

    def _browse(self):
        d = QFileDialog.getExistingDirectory(self, "Choose folder", self.dir_edit.text())
        if d:
            self.dir_edit.setText(d)
            self.settings.setValue("out_dir", d)


# ---------------------------------------------------------------- window
class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle(APP_NAME)
        self.resize(1120, 720)
        self.setMinimumSize(900, 600)
        self.settings = QSettings("YTDownloader", "YTDownloader")

        central = QWidget()
        root = QHBoxLayout(central)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(0)

        sidebar = QFrame()
        sidebar.setObjectName("Sidebar")
        sidebar.setFixedWidth(236)
        side = QVBoxLayout(sidebar)
        side.setContentsMargins(14, 22, 14, 16)
        side.setSpacing(4)
        side.addWidget(label("YT Downloader", "Brand"))
        side.addWidget(label("Your videos, offline", muted=True))
        side.addSpacing(18)

        self.stack = QStackedWidget()
        self.download_page = DownloadPage(self.settings)
        self.downloads_page = DownloadsPage()
        self.settings_page = SettingsPage(self.settings)
        for page in (self.download_page, self.downloads_page, self.settings_page):
            self.stack.addWidget(page)

        self.nav = QButtonGroup(self)
        self.nav_buttons = []
        for i, (text, icon_name) in enumerate([("Download", "download"), ("Downloads", "list"),
                                               ("Settings", "settings")]):
            b = QToolButton()
            b.setText(text)
            b.setProperty("nav", True)
            b.setCheckable(True)
            b.setIcon(icon(icon_name, Colors.TEXT, 22))
            b.setIconSize(QSize(22, 22))
            b.setToolButtonStyle(Qt.ToolButtonTextBesideIcon)
            b.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)
            b.setCursor(Qt.PointingHandCursor)
            self.nav.addButton(b, i)
            self.nav_buttons.append(b)
            side.addWidget(b)
        self.nav.idClicked.connect(self.show_page)
        side.addStretch(1)
        side.addWidget(label(f"v{APP_VERSION} · yt-dlp {yt_dlp.version.__version__}", muted=True))

        root.addWidget(sidebar)
        root.addWidget(self.stack, 1)
        self.setCentralWidget(central)

        self.download_page.download_requested.connect(self._enqueue)
        self.downloads_page.active_changed.connect(self._on_active)
        self.show_page(0)

        icon_file = resource_path("assets", "icon.ico")
        if os.path.isfile(icon_file):
            self.setWindowIcon(QIcon(icon_file))

    def show_page(self, index):
        self.nav_buttons[index].setChecked(True)
        self.stack.setCurrentIndex(index)
        if index == 0:
            self.download_page.url_edit.setFocus()
        elif index == 2:
            self.settings_page.dir_edit.setText(self.settings.value("out_dir", DEFAULT_DIR))

    def _enqueue(self, job):
        self.downloads_page.add(job)
        self.show_page(1)

    def _on_active(self, count):
        self.nav_buttons[1].setText(f"Downloads  ({count})" if count else "Downloads")

    def showEvent(self, event):
        super().showEvent(event)
        use_dark_title_bar(self)

    def closeEvent(self, event):
        active = self.downloads_page.active_count()
        if active:
            answer = QMessageBox.question(
                self, APP_NAME, f"There are {active} active downloads. Exit and cancel them?",
                QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
            if answer != QMessageBox.Yes:
                event.ignore()
                return
            self.downloads_page.cancel_all()
        event.accept()


def main():
    if sys.platform == "win32":
        import ctypes
        ctypes.windll.shell32.SetCurrentProcessExplicitAppUserModelID("YTDownloader.App")
    app = QApplication(sys.argv)
    app.setApplicationName(APP_NAME)
    apply_theme(app)
    icon_file = resource_path("assets", "icon.ico")
    if os.path.isfile(icon_file):
        app.setWindowIcon(QIcon(icon_file))
    win = MainWindow()
    win.show()
    if "--smoke-test" in sys.argv:  # installer smoke test: open the window and exit successfully
        QTimer.singleShot(1500, app.quit)
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
