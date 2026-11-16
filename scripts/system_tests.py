#!/usr/bin/env python3
"""Black-box tests of the real Rust collector, through TCP and HTTP."""
import json
import os
import socket
import struct
import subprocess
import time
import unittest
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SUFFIX = ".exe" if os.name == "nt" else ""
BINARY = ROOT / "rust" / "target" / "debug" / ("bms-collector" + SUFFIX)

def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]

def frame(seq=1, device=1, timestamp=None):
    return struct.pack(">4sHQQhHHBH", b"BMS1", device, seq,
                       timestamp if timestamp is not None else int(time.time()*1000),
                       235, 1020, 450, 7, 500)

class CollectorContract(unittest.TestCase):
    def setUp(self):
        self.ingest, self.http = free_port(), free_port()
        self.process = subprocess.Popen([str(BINARY), "--ingest-port", str(self.ingest),
            "--http-port", str(self.http), "--capacity", "2"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.addCleanup(self.stop)
        end = time.monotonic()+8
        while time.monotonic()<end:
            try:
                self.get("/healthz")
                break
            except (OSError, urllib.error.URLError):
                time.sleep(.04)
        else:
            self.fail("collector did not become ready")

    def stop(self):
        self.process.terminate()
        try:
            self.process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait(timeout=3)

    def get(self, path, raw=False):
        with urllib.request.urlopen(f"http://127.0.0.1:{self.http}{path}", timeout=4) as r:
            data = r.read()
            return data.decode() if raw else json.loads(data)

    def send(self, *frames):
        with socket.create_connection(("127.0.0.1", self.ingest), timeout=2) as s:
            s.sendall(b"".join(frames))

    def wait_count(self, accepted, rejected=0):
        end = time.monotonic()+3
        while time.monotonic()<end:
            data = self.get("/v1/latest")
            if data["accepted"] == accepted and data["rejected"] == rejected:
                return data
            time.sleep(.02)
        self.fail(f"counter mismatch: {data}")

    def test_health_empty_and_static_assets(self):
        self.assertEqual(self.get("/healthz"), {"status":"ok"})
        self.assertEqual(self.get("/v1/latest")["devices"], [])
        for path, marker in [("/", "BuildingEdge"), ("/app.js", "/v1/latest"), ("/app.css", "sidebar")]:
            self.assertIn(marker, self.get(path, raw=True))
        with self.assertRaises(urllib.error.HTTPError) as e:
            self.get("/missing")
        self.assertEqual(e.exception.code, 404)

    def test_bounded_history_and_replay(self):
        self.send(frame(1), frame(2), frame(3), frame(2))
        data=self.wait_count(3,1)
        self.assertEqual(data["history_size"], 2)
        self.assertEqual(data["evicted"], 1)
        self.assertEqual([s["seq"] for s in self.get("/v1/history")["samples"]], ["2","3"])
        self.assertEqual(data["devices"][0]["co2_ppm"], 1020)
        self.assertFalse(data["devices"][0]["stale"])

    def test_invalid_frame_and_fragmented_ingest(self):
        invalid=bytearray(frame()); invalid[28]=128
        self.send(bytes(invalid))
        with socket.create_connection(("127.0.0.1", self.ingest), timeout=2) as s:
            raw=frame()
            for part in (raw[:3],raw[3:15],raw[15:]):
                s.sendall(part)
                time.sleep(.03)
        self.wait_count(1,1)

    def test_stale_uses_receipt_clock_not_sender_timestamp(self):
        self.send(frame(timestamp=2**63))
        data=self.wait_count(1)
        self.assertFalse(data["devices"][0]["stale"])
        time.sleep(3.1)
        self.assertTrue(self.get("/v1/latest")["devices"][0]["stale"])
        self.assertIn('bms_device_sample_age_seconds{device="1"}', self.get("/metrics", raw=True))

    def test_fragmented_http_and_method(self):
        with socket.create_connection(("127.0.0.1",self.http), timeout=4) as s:
            for part in (b"GET /v1/",b"latest HTTP/1.1\r\nHost: local\r\n",b"\r\n"):
                s.sendall(part)
                time.sleep(.03)
            result=b""
            while True:
                piece=s.recv(8192)
                if not piece:break
                result+=piece
        self.assertIn(b"HTTP/1.1 200 OK", result)
        self.assertIn(b"Content-Security-Policy:", result)
        self.assertEqual(json.loads(result.split(b"\r\n\r\n",1)[1])["devices"], [])
        req=urllib.request.Request(f"http://127.0.0.1:{self.http}/v1/latest", data=b"", method="POST")
        with self.assertRaises(urllib.error.HTTPError) as e:
            urllib.request.urlopen(req, timeout=4)
        self.assertEqual(e.exception.code,405)

    def test_http_connection_limit_recovers(self):
        held=[]
        try:
            for _ in range(40):
                held.append(socket.create_connection(("127.0.0.1",self.http), timeout=2))
            time.sleep(.15)
        finally:
            for s in held:s.close()
        end=time.monotonic()+5
        while time.monotonic()<end:
            try:
                self.assertEqual(self.get("/healthz")["status"],"ok")
                return
            except OSError:
                time.sleep(.05)
        self.fail("HTTP workers did not release their permits")

if __name__ == "__main__":
    unittest.main(verbosity=2)
