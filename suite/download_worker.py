"""Headless yt-dlp engine. Requests and events use one JSON object per line."""
import json
import os
import sys
import threading
import shutil
import re
from pathlib import Path
from urllib.parse import urlsplit

import yt_dlp
from yt_dlp.postprocessor import PostProcessor


def emit_event(**event):
    print(json.dumps(event, ensure_ascii=True), flush=True)


class CompletedFile(PostProcessor):
    def __init__(self, downloader, paths):
        super().__init__(downloader)
        self.paths = paths

    def run(self, info):
        path = info.get("filepath")
        if path and os.path.isfile(path):
            self.paths.append(path)
        return [], info


if __name__ == "__main__":
    try:
        request = json.loads(sys.stdin.readline())
        url = urlsplit(request["url"])
        if url.scheme not in ("http", "https") or not url.hostname:
            raise ValueError("Use um link válido começando com https:// ou http://.")
        cancelled = threading.Event()
        options = {"quiet": True, "no_warnings": True, "noprogress": True,
                   "js_runtimes": {"deno": {}, "node": {}}, "windowsfilenames": True}
        if request.get("cookies"):
            if not Path(request["cookies"]).is_file():
                raise ValueError("O arquivo de cookies não foi encontrado.")
            options["cookiefile"] = request["cookies"]
        ffmpeg = shutil.which("ffmpeg")
        if ffmpeg:
            options["ffmpeg_location"] = os.path.dirname(ffmpeg)
        if request["action"] == "info":
            options.update(skip_download=True, noplaylist=True, extract_flat="in_playlist")
            with yt_dlp.YoutubeDL(options) as downloader:
                info = downloader.extract_info(request["url"], download=False)
            if not info:
                raise ValueError("Nenhum vídeo foi encontrado neste link.")
            heights = sorted({f["height"] for f in info.get("formats", []) if f.get("height")}, reverse=True)
            playlist = info.get("_type") in ("playlist", "multi_video")
            thumbnail = info.get("thumbnail") or next((t["url"] for t in reversed(info.get("thumbnails") or []) if t.get("url")), "")
            emit_event(type="info", title=info.get("title") or info.get("id") or url.hostname,
                       channel=info.get("uploader") or info.get("channel") or "", source=info.get("extractor_key") or url.hostname,
                       duration=info.get("duration"), thumbnail=thumbnail, heights=heights, playlist=playlist,
                       count=info.get("playlist_count") or (len(info["entries"]) if isinstance(info.get("entries"), list) else 0))
            sys.exit(0)

        def on_cancel_callback():
            for line in sys.stdin:
                if line.strip() == "cancel":
                    cancelled.set()
                    return

        def on_progress_callback(event):
            if cancelled.is_set():
                raise RuntimeError("Cancelado")
            if event["status"] == "finished":
                emit_event(type="progress", percent=-1, message="Processando vídeo e áudio…")
                return
            total = event.get("total_bytes") or event.get("total_bytes_estimate") or 0
            done = event.get("downloaded_bytes") or 0
            percent = done * 100 / total if total else -1
            message = f"Baixando · {percent:.0f}%" if total else "Baixando…"
            if event.get("speed"):
                message += f" · {event['speed'] / 1024 / 1024:.1f} MB/s"
            if event.get("eta") is not None:
                message += f" · faltam {int(event['eta']) // 60}:{int(event['eta']) % 60:02d}"
            index = (event.get("info_dict") or {}).get("playlist_index")
            if index:
                message = f"Vídeo {index} · {message}"
            emit_event(type="progress", percent=percent, message=message)

        threading.Thread(target=on_cancel_callback, daemon=True).start()
        audio = request["audio"]
        height = request["height"]
        if audio and not ffmpeg:
            raise RuntimeError("FFmpeg não encontrado. Instale o FFmpeg para converter para MP3.")
        limit = f"[height<={height}]" if height else ""
        options["format"] = f"best[ext=mp4]{limit}/best{limit}/best"
        if ffmpeg:
            options["format"] = f"bestvideo[ext=mp4]{limit}+bestaudio[ext=m4a]/bestvideo{limit}+bestaudio/best{limit}/best"
            options["merge_output_format"] = "mp4"
        if audio:
            options["format"] = "bestaudio/best"
            options["postprocessors"] = [{"key": "FFmpegExtractAudio", "preferredcodec": "mp3", "preferredquality": "192"}]
        template = "%(playlist_title)s/%(playlist_index)03d - %(title)s.%(ext)s" if request["playlist"] else "%(title)s.%(ext)s"
        options.update(outtmpl=str(Path(request["directory"]) / template),
                       noplaylist=not request["playlist"], progress_hooks=[on_progress_callback])
        paths = []
        emit_event(type="state", state="running", message="Conectando…")
        with yt_dlp.YoutubeDL(options) as downloader:
            downloader.add_post_processor(CompletedFile(downloader, paths), when="after_move")
            downloader.extract_info(request["url"], download=True)
        if cancelled.is_set():
            emit_event(type="state", state="cancelled", message="Cancelado")
            sys.exit(0)
        if not paths:
            raise RuntimeError("Nenhum arquivo foi concluído.")
        result = os.path.commonpath([str(Path(path).parent) for path in paths]) if request["playlist"] or len(paths) > 1 else paths[0]
        emit_event(type="state", state="done", message="Concluído", path=result)
    except Exception as error:
        message = re.sub(r"\x1b\[[0-9;]*m", "", str(error))
        if "Unsupported URL" in message:
            message = "Não foi encontrado um vídeo compatível neste link. O site pode não ser suportado.\n" + message
        if any(text in message.lower() for text in ("sign in", "login required", "private video", "permission denied")):
            message = "Este conteúdo exige acesso. Escolha um arquivo de cookies de uma conta com permissão.\n" + message
        emit_event(type="state", state="error", message=message)
        sys.exit(1)
