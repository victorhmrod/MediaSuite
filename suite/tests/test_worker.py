"""Exercise the source or packaged download engine against local media."""
import functools
import http.server
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import unittest


class MediaServer(http.server.SimpleHTTPRequestHandler):
    extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map, ".m3u8": "application/vnd.apple.mpegurl"}

    def do_GET(self):
        if self.path == "/protected.mp4":
            if "session=allowed" not in self.headers.get("Cookie", ""):
                self.send_error(403)
                return
            self.path = "/fixture.mp4"
        super().do_GET()

    def log_message(self, *args):
        pass


class WorkerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.root = Path(cls.temp.name)
        subprocess.run(["ffmpeg", "-v", "error", "-f", "lavfi", "-i", "color=c=blue:s=160x90:r=10",
                        "-f", "lavfi", "-i", "sine=frequency=440", "-t", "0.5",
                        "-c:v", "libx264", "-c:a", "aac", str(cls.root / "fixture.mp4")], check=True)
        (cls.root / "watch.html").write_text('<html><head><title>Embedded video</title></head><body><video src="/fixture.mp4" controls></video></body></html>')
        (cls.root / "second.mp4").write_bytes((cls.root / "fixture.mp4").read_bytes())
        (cls.root / "playlist.xspf").write_text('<?xml version="1.0"?><playlist version="1" xmlns="http://xspf.org/ns/0/"><title>Test playlist</title><trackList><track><location>fixture.mp4</location><title>First</title></track><track><location>second.mp4</location><title>Second</title></track></trackList></playlist>')
        subprocess.run(["ffmpeg", "-v", "error", "-i", str(cls.root / "fixture.mp4"), "-c", "copy", "-hls_time", "0.25", "-hls_list_size", "0", str(cls.root / "stream.m3u8")], check=True)
        subprocess.run(["ffmpeg", "-v", "error", "-i", "fixture.mp4", "-c", "copy", "-f", "dash", "stream.mpd"], cwd=cls.root, check=True)
        cls.server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), functools.partial(MediaServer, directory=cls.temp.name))
        threading.Thread(target=cls.server.serve_forever, daemon=True).start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.temp.cleanup()

    def run_worker(self, audio=False, file="fixture.mp4", action="download", cookies=None, playlist=False):
        executable = os.environ.get("MEDIA_SUITE_TEST_WORKER")
        command = [executable] if executable else [sys.executable, "-u", str(Path(__file__).parents[1] / "download_worker.py")]
        request = {"action": action, "url": f"http://127.0.0.1:{self.server.server_port}/{file}",
                   "directory": str(self.root / ("áudio com espaços" if audio else "vídeo com espaços")),
                   "audio": audio, "height": 0, "playlist": playlist, "cookies": str(cookies) if cookies else ""}
        result = subprocess.run(command, input=json.dumps(request) + "\n", capture_output=True, text=True, timeout=45)
        events = [json.loads(line) for line in result.stdout.splitlines() if line.startswith("{")]
        self.assertTrue(events, result.stderr)
        return result, events

    def test_video_output_and_progress(self):
        result, events = self.run_worker()
        self.assertEqual(result.returncode, 0, events)
        self.assertTrue(any(event["type"] == "progress" for event in events))
        self.assertEqual(events[-1]["state"], "done")
        output = Path(events[-1]["path"])
        self.assertTrue(output.is_file())
        self.assertEqual(output.suffix, ".mp4")

    def test_audio_postprocessing_returns_final_mp3(self):
        result, events = self.run_worker(audio=True)
        self.assertEqual(result.returncode, 0, events)
        self.assertEqual(events[-1]["state"], "done")
        output = Path(events[-1]["path"])
        self.assertTrue(output.is_file())
        self.assertEqual(output.suffix, ".mp3")

    def test_failed_download_never_reports_success(self):
        result, events = self.run_worker(file="missing.mp4")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(events[-1]["state"], "error")
        self.assertFalse(any(event.get("state") == "done" for event in events))

    def test_metadata_for_direct_video(self):
        result, events = self.run_worker(action="info")
        self.assertEqual(result.returncode, 0, events)
        self.assertEqual(events[-1]["type"], "info")
        self.assertTrue(events[-1]["title"])
        self.assertTrue(events[-1]["source"])

    def test_video_embedded_in_webpage(self):
        result, events = self.run_worker(file="watch.html")
        self.assertEqual(result.returncode, 0, events)
        self.assertEqual(events[-1]["state"], "done")
        self.assertTrue(Path(events[-1]["path"]).is_file())

    def test_hls_stream(self):
        result, events = self.run_worker(file="stream.m3u8")
        self.assertEqual(result.returncode, 0, events)
        self.assertEqual(events[-1]["state"], "done")
        self.assertTrue(Path(events[-1]["path"]).is_file())

    def test_dash_stream(self):
        result, events = self.run_worker(file="stream.mpd")
        self.assertEqual(result.returncode, 0, events)
        self.assertEqual(events[-1]["state"], "done")
        self.assertTrue(Path(events[-1]["path"]).is_file())

    def test_playlist_preview_and_download(self):
        result, events = self.run_worker(file="playlist.xspf", action="info")
        self.assertEqual(result.returncode, 0, events)
        self.assertTrue(events[-1]["playlist"])
        self.assertEqual(events[-1]["count"], 2)
        result, events = self.run_worker(file="playlist.xspf", playlist=True)
        self.assertEqual(result.returncode, 0, events)
        self.assertEqual(events[-1]["state"], "done")
        self.assertEqual(len(list(Path(events[-1]["path"]).glob("*.mp4"))), 2)

    def test_authenticated_download_with_selected_cookies(self):
        cookies = self.root / "cookies.txt"
        cookies.write_text("# Netscape HTTP Cookie File\n127.0.0.1\tFALSE\t/\tFALSE\t2147483647\tsession\tallowed\n")
        result, events = self.run_worker(file="protected.mp4", action="info", cookies=cookies)
        self.assertEqual(result.returncode, 0, events)
        self.assertEqual(events[-1]["type"], "info")
        result, events = self.run_worker(file="protected.mp4", cookies=cookies)
        self.assertEqual(result.returncode, 0, events)
        self.assertEqual(events[-1]["state"], "done")
        self.assertTrue(Path(events[-1]["path"]).is_file())

    def test_missing_cookie_file(self):
        result, events = self.run_worker(action="info", cookies=self.root / "missing-cookies.txt")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("cookies", events[-1]["message"])


if __name__ == "__main__":
    unittest.main()
