#!/usr/bin/env python3
import sys
import subprocess
import time
import urllib.request
import urllib.error
import json
import os
import socket

def run_tests():
    if len(sys.argv) < 3:
        print("Usage: RunAssetdWebSocketTests.py <path_to_assetd> <path_to_arp_pcap>")
        sys.exit(1)

    assetd_path = sys.argv[1]
    pcap_path = sys.argv[2]

    # Setup temporary files
    db_path = "test-ws.db"
    log_path = "test-ws.log"
    if os.path.exists(db_path):
        os.remove(db_path)
    if os.path.exists(log_path):
        os.remove(log_path)

    # Start assetd in the background
    cmd = [
        assetd_path,
        "--serve",
        "--listen-address", "127.0.0.1",
        "--port", "18081",
        "--sqlite", db_path,
        "--runtime-log", log_path,
        "--capture-mode", "pcap",
        "--pcap", pcap_path
    ]

    print(f"Starting assetd: {' '.join(cmd)}")
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    # Wait for the service to start
    time.sleep(0.5)

    def connect_ws():
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(("127.0.0.1", 18081))
        handshake = (
            "GET /ws/events HTTP/1.1\r\n"
            "Host: 127.0.0.1:18081\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            "Sec-WebSocket-Version: 13\r\n\r\n"
        )
        s.sendall(handshake.encode("utf-8"))
        resp = b""
        while b"\r\n\r\n" not in resp:
            chunk = s.recv(1024)
            if not chunk:
                break
            resp += chunk
        assert b"101 Switching Protocols" in resp
        return s

    def recv_ws_frame(s):
        header = s.recv(2)
        if len(header) < 2:
            return None
        b1, b2 = header[0], header[1]
        opcode = b1 & 0x0F
        masked = (b2 & 0x80) != 0
        length = b2 & 0x7F
        if length == 126:
            len_bytes = s.recv(2)
            length = int.from_bytes(len_bytes, byteorder="big")
        elif length == 127:
            len_bytes = s.recv(8)
            length = int.from_bytes(len_bytes, byteorder="big")

        if masked:
            mask = s.recv(4)

        payload = b""
        while len(payload) < length:
            chunk = s.recv(length - len(payload))
            if not chunk:
                break
            payload += chunk

        if masked:
            payload = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))

        if opcode == 1:
            return payload.decode("utf-8")
        return payload

    ws_sock = None
    try:
        # Connect WebSocket client
        print("Connecting WebSocket client...")
        ws_sock = connect_ws()

        # Start capture via POST request
        print("Sending POST /api/v1/capture/start...")
        req = urllib.request.Request("http://127.0.0.1:18081/api/v1/capture/start", method="POST")
        with urllib.request.urlopen(req, timeout=2) as response:
            assert response.status == 200
            res = json.loads(response.read().decode('utf-8'))
            assert res["success"] is True

        # Read streamed events
        print("Reading streamed events...")
        event_types_received = set()

        # We expect capture.started, event.detected, asset.created, metrics.updated, capture.stopped
        # Wait up to 5 seconds to receive events
        start_time = time.time()
        while time.time() - start_time < 5.0:
            frame = recv_ws_frame(ws_sock)
            if frame is None:
                break
            try:
                event = json.loads(frame)
                print(f"Received event: {event['type']}")
                event_types_received.add(event["type"])
                if event["type"] == "capture.stopped":
                    break
            except Exception as e:
                print(f"Failed to parse event frame: {e}")

        # Assert we received the expected events
        expected_types = {"capture.started", "event.detected", "asset.created", "metrics.updated", "capture.stopped"}
        missing_types = expected_types - event_types_received
        assert len(missing_types) == 0, f"Missing expected events: {missing_types}. Received: {event_types_received}"
        print("All expected WebSocket events received successfully!")

    finally:
        if ws_sock:
            ws_sock.close()
        # Terminate daemon
        print("Terminating assetd...")
        proc.terminate()
        try:
            proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()

        # Cleanup
        if os.path.exists(db_path):
            os.remove(db_path)
        if os.path.exists(log_path):
            os.remove(log_path)

if __name__ == "__main__":
    run_tests()
