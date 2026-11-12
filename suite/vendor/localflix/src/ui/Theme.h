#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QSize>
#include <QString>

class QApplication;
class QWidget;

namespace lf::ui {

enum class Icon {
    Home,
    Continue,
    Tv,
    Movie,
    Collections,
    Settings,
    Play,
    Pause,
    Stop,
    Next,
    Previous,
    Rewind,
    Forward,
    VolumeUp,
    VolumeOff,
    Fullscreen,
    FullscreenExit,
    Subtitles,
    Audio,
    Speed,
    Back,
    Add,
    Folder,
    Edit,
    Delete,
    Check,
    More,
    Close,
    Image,
    Refresh,
    Warning,
    Replay,
    Flag,
    PictureInPicture
};

namespace Colors {
inline const QColor background(0x0b, 0x0e, 0x13);
inline const QColor surface(0x12, 0x17, 0x20);
inline const QColor surfaceRaised(0x1b, 0x23, 0x2f);
inline const QColor border(0x30, 0x3c, 0x4d);
inline const QColor text(0xee, 0xf4, 0xfc);
inline const QColor textMuted(0x9c, 0xac, 0xc1);
inline const QColor accent(0x6b, 0xc7, 0xff);
inline const QColor accentHover(0xa3, 0xde, 0xff);
inline const QColor success(0x3e, 0xcf, 0x8e);
inline const QColor warning(0xf5, 0xa5, 0x24);
inline const QColor danger(0xef, 0x53, 0x50);
}  // namespace Colors

namespace Theme {

// Dark palette, chamfered buttons and animated interaction states.
void apply(QApplication& app);

// Vector icon (drawn from embedded SVG paths) tinted with `color`.
QPixmap iconPixmap(Icon icon, const QColor& color, int logicalSize, qreal devicePixelRatio = 1.0);
QIcon icon(Icon icon, const QColor& color = Colors::text, int logicalSize = 24);

// Cover/poster scaled and cropped to `size` (cover-fit). Falls back to a
// generated gradient placeholder that carries the title's initials.
QPixmap cover(const QString& imagePath, const QString& title, const QSize& size, qreal devicePixelRatio = 1.0);

// Enables the dark title bar on Windows 10/11 for this top-level window.
void useDarkTitleBar(QWidget* topLevel);

}  // namespace Theme

}  // namespace lf::ui
