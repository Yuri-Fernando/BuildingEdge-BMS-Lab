#!/usr/bin/env python3
"""Exercise C++ error handling against an independent TCP fault oracle."""
import json
import socketserver
import struct
import subprocess
import tempfile
import threading
import time
import unittest
from pathlib import Path
from integration import EDGE, ReusableTCPServer, recv_all

class FaultHandler(socketserver.BaseRequestHandler):
    def handle(self):
        self.request.settimeout(2)
        try:
            while True:
                hdr=recv_all(self.request,7)
                pdu=recv_all(self.request,struct.unpack_from(">H",hdr,4)[0]-1)
                if pdu != bytes([3,0,0,0,6]):return
                with self.server.lock:
                    self.server.count+=1
                    first=self.server.count==1
                mode=self.server.mode if first else "normal"
                registers=(235,1050,1001 if mode=="values" else 450,1,1,500)
                reply=hdr[:2]+bytes([0,0,0,15,1,3,12])+struct.pack(">6H",*registers)
                if mode=="tid":reply=bytes([hdr[0],hdr[1]^1])+reply[2:]
                if mode=="unit":reply=reply[:6]+bytes([2])+reply[7:]
                if mode=="exception":reply=hdr[:2]+bytes([0,0,0,3,1,0x83,2])
                if mode=="length":reply=reply[:4]+bytes([0,255])+reply[6:]
                if mode=="truncated":
                    self.request.sendall(reply[:10])
                    return
                if mode=="fragmented":
                    for part in (reply[:3],reply[3:8],reply[8:]):
                        self.request.sendall(part)
                        time.sleep(.005)
                else:self.request.sendall(reply)
        except (OSError,EOFError):return

class GatewayFaults(unittest.TestCase):
    def run_case(self,mode):
        server=ReusableTCPServer(("127.0.0.1",0),FaultHandler)
        server.mode=mode;server.count=0;server.lock=threading.Lock()
        threading.Thread(target=server.serve_forever,daemon=True).start()
        try:
            with tempfile.TemporaryDirectory() as temp:
                report=Path(temp)/"metrics.json"
                result=subprocess.run([str(EDGE),"--port",str(server.server_address[1]),
                    "--rounds","8","--poll-ms","2","--no-collector","--metrics-file",str(report)],
                    capture_output=True,text=True,timeout=15,check=True)
                metrics=json.loads(report.read_text())
                rows=[json.loads(line) for line in result.stdout.splitlines() if line.strip()]
                self.assertEqual(metrics["poll_success"],len(rows))
                self.assertGreaterEqual(len(rows),6,"gateway did not recover")
                if mode=="fragmented":
                    self.assertEqual(metrics["modbus_errors"],0)
                    self.assertEqual(len(rows),8)
                elif mode=="values":
                    self.assertEqual(metrics["validation_errors"],1)
                else:self.assertGreater(metrics["modbus_errors"],0)
                return metrics
        finally:
            server.shutdown();server.server_close()
    def test_wrong_transaction_recovers(self):self.run_case("tid")
    def test_wrong_unit_recovers(self):self.run_case("unit")
    def test_exception_recovers(self):self.run_case("exception")
    def test_oversized_length_recovers(self):self.run_case("length")
    def test_truncated_response_recovers(self):self.run_case("truncated")
    def test_invalid_measurement_is_not_published(self):self.run_case("values")
    def test_fragmented_tcp_response(self):self.run_case("fragmented")

if __name__=="__main__":unittest.main(verbosity=2)
