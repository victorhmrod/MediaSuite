# Media Suite guide

Media Suite combines YT Downloader, LocalFlix, and MagnifyFactory in a single
Windows application. All interface text, download messages, build instructions,
and installer prompts are in English.

## Using the application

Run the installed Start Menu shortcut, or extract the portable ZIP and open
`MediaSuite.exe`. The installer supports per-user installation without
administrator privileges and optional all-user installation. Uninstalling removes
the application while keeping your library, preferences, and media files.

The sidebar provides access to Home, Continue Watching, TV shows, Movies and
videos, Collections, Settings, Download videos, and Convert and edit.

### Download videos

Paste an HTTP/HTTPS link and select **Look up**. The application detects the
source, displays a preview when available, and lists the reported quality options.
Choose an output format, a destination folder, and **Download**. **Keep downloaded
format** saves the source without conversion. Selecting video (MP4, MKV, WebM,
MOV, AVI, WMV, MPEG, FLV), audio (MP3, WAV, FLAC, AAC, M4A, OGG), or an image
format starts the integrated converter after download. Image outputs use the
first frame, including GIF. Only compatible media formats are offered.

Sources include YouTube, Google Drive, direct media files, videos embedded in
websites, and non-DRM HLS/DASH streams supported by yt-dlp. Drive file links such
as `file/d/...`, `open?id=...`, and `uc?id=...` use the GoogleDrive extractor.
Playlists and shared folders are detected automatically when the source supports
them. Compatibility depends on the website, extractor, and account permissions.
See the [supported sources](https://github.com/yt-dlp/yt-dlp/blob/master/supportedsites.md).

For content that requires sign-in, use **Choose cookies** to select a
Netscape/Mozilla-format cookies file from an account with access. The same file
is used for lookup and download. Cookies are not extracted from browsers
automatically. Private content requires permission; DRM and server restrictions
can prevent downloading.

Up to three downloads run at once. Each job supports cancellation, retry,
opening the result, showing its folder, importing video into the library, and
converting directly on the download card. You can change the card's format
and convert again after completion. Playlist downloads convert every downloaded
file. Conversion progress, errors, cancellation, and retry remain on that page;
the same jobs appear in **Convert and edit**. Retrying a failed conversion reuses
the downloaded file. Originals and existing output files are preserved. Automatic
library import adds successful video outputs, not audio or still images.

### Application updates (private repository)

Open **Settings → Application updates** and save a fine-grained GitHub token
with **Contents: read** access to `victorhmrod/MediaSuite`. The application stores
the token in Windows Credential Manager, not in its settings files or logs.
See [GitHub's release API permissions](https://docs.github.com/en/rest/releases/releases#get-the-latest-release).
Leaving the token field blank and choosing **Save token** removes it.

**Check for updates** checks the latest stable release. Automatic discovery is
enabled at startup and can be disabled with the checkbox. A newer release
produces a notification; **Download update** fetches its Windows installer.
The size and SHA-256 digest must match GitHub's release asset metadata. The
application rechecks the file before installation and never forwards the token
to the signed asset CDN URL.

Select **Install update** when ready, then confirm closing Media Suite. Active
download or conversion jobs must finish or be cancelled first. The installer
retains the existing per-user/all-user installation location when recognized;
Windows may request elevation for an all-user installation. Discovery never
installs an update without confirmation. Install version 1.3.0 once to enable
this feature for subsequent releases. This updates the application package,
including its bundled worker; it does not update external tools independently.

### Library, playback, and tools

Create shows, import movie files or folders, arrange episodes, and group items
into collections. The player remembers progress and supports automatic next
episodes, chapter-based opening/ending skips, and picture in picture.

The converter retains MagnifyFactory engines, presets, watch folders, and video,
audio, image, PDF, document, and 3D editors. Right-click a completed video job
and select **Add to library** to import its output.

## Dependencies

Published packages bundle Qt, Python, yt-dlp, FFmpeg, FFprobe, and libmpv.
Node.js or Deno on PATH enables JavaScript challenges used by some websites.
PDF tools may require Poppler and QPDF; document conversion uses LibreOffice;
archive tools use 7-Zip. Individual 3D formats may require additional converters.

## Data and preferences

The new library lives in `%APPDATA%\MediaSuite`, separate from LocalFlix.
Importing existing media does not copy, move, or delete video files. Download
folder preferences from YT Downloader are reused.

`LOCALFLIX_DATA_DIR` selects another library folder. Close applications using
that folder before opening it here. Tests use isolated library and download
preferences. `MEDIA_SUITE_SETTINGS_ORGANIZATION` can isolate downloader settings.

## Building

Requirements: Windows x64, MSVC C++20, Ninja, CMake 3.24+, and Qt 6.7+ with
Widgets, Sql, Svg, OpenGLWidgets, Concurrent, Network, and Test components.

```powershell
git clone https://github.com/victorhmrod/MediaSuite.git
cd MediaSuite
python -m pip install 'yt-dlp[default]' pyinstaller
# Requires 7-Zip; downloads libmpv and verifies its SHA-256.
.\scripts\fetch-suite-deps.ps1
$env:LOCALFLIX_QT_DIR = 'D:\Qt\6.7.3\msvc2019_64'
.\scripts\build-suite.cmd
```

The default Qt kit is `D:\Qt\6.7.3\msvc2019_64`. Set `LOCALFLIX_QT_DIR` for
another location. libmpv defaults to `third_party/libmpv`; configure
`-DLOCALFLIX_LIBMPV_DIR=C:/path/to/libmpv` in `build/suite` to override it.
All three applications' sources are included in this repository.

The application icon is defined in `assets/media-suite.svg`: a white clapperboard
on a black background. `python tools/make_media_suite_icon.py` regenerates its
PNG and multi-resolution Windows ICO assets (requires PySide6 and Pillow).

For development, put Qt's `bin` directory on PATH and install `yt-dlp[default]`
in Python. `MEDIA_SUITE_PYTHON` selects the Python executable. Distributed builds
use `worker/download-worker.exe` instead.

## Packaging and release

Install Inno Setup 6, then run:

```powershell
.\build.cmd
# Or reuse an existing build and package:
.\scripts\make-suite-release.ps1 -SkipPackage
```

The release script builds the portable package, compiles
`installer/MediaSuite.iss`, creates SHA-256 checksums, and verifies silent
installation, application startup, and uninstallation in an isolated directory.
It refuses to run the installation test over an existing Media Suite install.
`MEDIA_SUITE_ISCC` selects a custom Inno Setup compiler.

Outputs for version 1.3.0:

- `dist/MediaSuite-1.3.0/MediaSuite.exe`
- `dist/MediaSuite-1.3.0-win64.zip`
- `dist/MediaSuite-1.3.0-setup.exe`
- `dist/SHA256SUMS.txt`

`package-suite.ps1` creates only the application folder and portable ZIP.
The original downloader release scripts remain available separately.

## Validation

```powershell
ctest --test-dir build/suite --output-on-failure
python -m unittest discover -s suite/tests -p test_worker.py -v
```

`tst_suite` exercises HTTP lookup/download through the interface, automatic and
manual conversion, video/audio/image outputs, conversion failure/retry, playlist
conversion, collision-safe output paths, library import, and navigation.
`tst_updates` tests isolated Windows credential storage, startup discovery,
version comparison, authenticated downloads, SHA-256 checks, corrupted/tampered
installers, unauthorized requests, and rejected redirects. Python tests cover video, MP3, direct-link
metadata, HTML embeds, HLS, DASH, playlists, cookies, and errors. Set
`MEDIA_SUITE_TEST_WORKER` to test the packaged worker. Playback regression tests
use synthetic clips from `MEDIA_SUITE_TESTDATA_DIR`, configurable through CMake;
those tests skip if clips are unavailable.

`--downloads` and `--tools` open their respective pages. `--settings` opens
Settings. `--screenshot file.png` saves a window capture and exits. This
integration targets Windows; upstream Android applications are not included.

## Licenses

The combined application is distributed under AGPL-3.0. LocalFlix and
MagnifyFactory license notices remain in `vendor/`. See
`THIRD_PARTY_NOTICES.md` for third-party details.
