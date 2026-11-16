#!/usr/bin/env python3
"""End-to-end protocol checks: Python mock locally or real Rust processes in CI.

Requires a previously built C++ executable and, in --rust mode, Cargo build.
Only loopback is used. No production devices are accessed.
"""
import argparse
import json
import socket
import socketserver
import struct
import subprocess
import tempfile
import threading
import time
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EDGE = ROOT / "build" / "bms-edge"


def recv_all(sock, n):
    result = bytearray()
    while len(result) < n:
        piece = sock.recv(n - len(result))
        if not piece:
            raise EOFError()
        result.extend(piece)
    return bytes(result)


class SimState:
    def __init__(self, drop_every):
        self.requests = 0
        self.drop_every = drop_every
        self.lock = threading.Lock()


class SimHandler(socketserver.BaseRequestHandler):
    def handle(self):
        self.request.settimeout(2)
        try:
            while True:
                header = recv_all(self.request, 7)
                length = struct.unpack_from(">H", header, 4)[0]
                if length < 2 or length > 253:
                    return
                pdu = recv_all(self.request, length - 1)
                assert length == 6 and pdu == b"\x03\x00\x00\x00\x06"
                state = self.server.state
                with state.lock:
                    state.requests += 1
                    n = state.requests
                if state.drop_every and n % state.drop_every == 0:
                    return
                phase = n % 40
                co2 = 850 + phase * 20 if phase < 20 else 1250 - (phase - 20) * 20
                registers = (230 + n % 10 - 5, co2, 450 + n % 7, int(n % 9 != 0), int(co2 >= 950), 250 + n % 100)
                reply = header[:2] + b"\x00\x00\x00\x0f\x01\x03\x0c" + struct.pack(">6H", *registers)
                self.request.sendall(reply)
        except (EOFError, BrokenPipeError, ConnectionError, socket.timeout):
            return


class CollectorState:
    def __init__(self):
        self.frames = []
        self.lock = threading.Lock()


class CollectorHandler(socketserver.BaseRequestHandler):
    def handle(self):
        self.request.settimeout(2)
        try:
            while True:
                raw = recv_all(self.request, 31)
                values = struct.unpack(">4sHQQhHHBH", raw)
                assert values[0] == b"BMS1"
                assert values[1] == 1
                assert -500 <= values[4] <= 1250
                assert 0 <= values[6] <= 1000
                with self.server.state.lock:
                    self.server.state.frames.append(values)
        except (EOFError, BrokenPipeError, ConnectionError, socket.timeout):
            return


class ReusableTCPServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def wait_port(port, timeout=5):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        try:
            with socket.create_connection(("127.0.0.1", port), 0.15):
                return
        except OSError:
            time.sleep(0.04)
    raise AssertionError(f"port {port} did not open")


def run_case(mode, drop_every):
    sim_state = SimState(drop_every)
    coll_state = CollectorState()
    processes, servers = [], []
    try:
        if mode == "python":
            sim = ReusableTCPServer(("127.0.0.1", 0), SimHandler)
            sim.state = sim_state
            sim_port = sim.server_address[1]
            coll = ReusableTCPServer(("127.0.0.1", 0), CollectorHandler)
            coll.state = coll_state
            collector_port = coll.server_address[1]
            for srv in (sim, coll):
                servers.append(srv)
                threading.Thread(target=srv.serve_forever, daemon=True).start()
        else:
            sim_port, collector_port, http_port = free_port(), free_port(), free_port()
            sim_bin = ROOT / "rust" / "target" / "debug" / "modbus-sim"
            coll_bin = ROOT / "rust" / "target" / "debug" / "bms-collector"
            processes.append(subprocess.Popen([str(sim_bin), "--port", str(sim_port), "--drop-every", str(drop_every)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL))
            processes.append(subprocess.Popen([str(coll_bin), "--ingest-port", str(collector_port), "--http-port", str(http_port), "--capacity", "32"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL))
            wait_port(sim_port)
            wait_port(collector_port)
            wait_port(http_port)
        with tempfile.TemporaryDirectory() as temp:
            metric_path = Path(temp) / "metrics.json"
            cmd = [str(EDGE), "--host", "127.0.0.1", "--port", str(sim_port),
                   "--collector", "127.0.0.1", "--collector-port", str(collector_port),
                   "--poll-ms", "5", "--rounds", "80", "--ring-capacity", "12",
                   "--metrics-file", str(metric_path)]
            p = subprocess.run(cmd, capture_output=True, text=True, timeout=40, check=True)
            metrics = json.loads(metric_path.read_text())
            rows = [json.loads(line) for line in p.stdout.splitlines() if line.strip()]
            assert metrics["poll_success"] == len(rows), metrics
            assert len(rows) >= (20 if drop_every else 75), metrics
            assert metrics["ring_evictions"] == max(0, len(rows) - 12), metrics
            assert metrics["poll_p99_us"] >= metrics["poll_p50_us"]
            assert any(row["co2_alarm"] for row in rows), "alarm never triggered"
            if drop_every:
                assert metrics["modbus_errors"] > 0, "fault injection had no effect"
            else:
                assert metrics["modbus_errors"] == 0, metrics
            if mode == "python":
                deadline = time.monotonic() + 2
                while time.monotonic() < deadline and len(coll_state.frames) < len(rows):
                    time.sleep(0.02)
                assert len(coll_state.frames) == len(rows), (len(coll_state.frames), len(rows))
                assert coll_state.frames[-1][3] > 0, "timestamp not set"
            else:
                deadline = time.monotonic() + 2
                data = {}
                while time.monotonic() < deadline:
                    with urllib.request.urlopen(f"http://127.0.0.1:{http_port}/v1/latest", timeout=2) as response:
                        data = json.load(response)
                    if data["accepted"] == len(rows):
                        break
                    time.sleep(0.02)
                assert data["accepted"] == len(rows), (data, len(rows))
                assert len(data["devices"]) == 1
                with urllib.request.urlopen(f"http://127.0.0.1:{http_port}/metrics", timeout=2) as response:
                    metrics_text = response.read().decode()
                assert "bms_samples_accepted_total" in metrics_text
            print(json.dumps({"mode": mode, "drop_every": drop_every, "metrics": metrics, "rows": len(rows)}, sort_keys=True))
    finally:
        for server in servers:
            server.shutdown()
            server.server_close()
        for p in processes:
            p.terminate()
            try:
                p.wait(timeout=2)
            except subprocess.TimeoutExpired:
                p.kill()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=["python", "rust"], default="python")
    args = parser.parse_args()
    run_case(args.mode, 0)
    run_case(args.mode, 7)
