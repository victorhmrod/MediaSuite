"""Render the flat Media Suite SVG into PNG and Windows ICO assets."""
from pathlib import Path

from PIL import Image
from PySide6.QtCore import Qt
from PySide6.QtGui import QGuiApplication, QImage, QPainter
from PySide6.QtSvg import QSvgRenderer

app = QGuiApplication([])
assets = Path(__file__).resolve().parents[1] / "assets"
renderer = QSvgRenderer(str(assets / "media-suite.svg"))
if not renderer.isValid():
    raise SystemExit("The Media Suite SVG is invalid.")
image = QImage(1024, 1024, QImage.Format_ARGB32)
image.fill(Qt.black)
painter = QPainter(image)
painter.setRenderHint(QPainter.Antialiasing)
renderer.render(painter)
painter.end()
if not image.save(str(assets / "media-suite.png")):
    raise SystemExit("Could not save the Media Suite PNG.")
with Image.open(assets / "media-suite.png") as icon:
    icon.save(assets / "media-suite.ico", sizes=[(size, size) for size in (16, 20, 24, 32, 40, 48, 64, 128, 256)])
print("Generated assets/media-suite.png and assets/media-suite.ico")
