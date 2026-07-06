#!/usr/bin/env python3
import sys
import subprocess
import time
import urllib.request
import urllib.error
import json
import os

def run_tests():
    if len(sys.argv) < 2:
        print("Usage: RunAssetdRestTests.py <path_to_assetd>")
        sys.exit(1)

    assetd_path = sys.argv[1]

    # Setup temporary files
    db_path = "test-rest.db"
    log_path = "test-runtime.log"
    if os.path.exists(db_path):
        os.remove(db_path)
    if os.path.exists(log_path):
        os.remove(log_path)

    # Start assetd in the background
    cmd = [
        assetd_path,
        "--serve",
        "--listen-address", "127.0.0.1",
        "--port", "18080",
        "--sqlite", db_path,
        "--runtime-log", log_path
    ]

    print(f"Starting assetd: {' '.join(cmd)}")
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    # Wait for the service to start
    time.sleep(0.5)

    def request(path, method="GET", data=None):
        url = f"http://127.0.0.1:18080{path}"
        req = urllib.request.Request(url, method=method)
        if data is not None:
            req.data = data.encode('utf-8')
            req.add_header('Content-Type', 'application/json')
        try:
            with urllib.request.urlopen(req, timeout=2) as response:
                status = response.status
                body = response.read().decode('utf-8')
                return status, json.loads(body)
        except urllib.error.HTTPError as e:
            body = e.read().decode('utf-8')
            try:
                err_json = json.loads(body)
            except Exception:
                err_json = {"raw": body}
            return e.code, err_json
        except Exception as e:
            print(f"Error connecting to {url}: {e}")
            raise e

    try:
        # Test 1: GET /api/v1/status
        print("Testing GET /api/v1/status...")
        status, res = request("/api/v1/status")
        assert status == 200, f"Expected 200, got {status}"
        assert res["success"] is True
        assert "data" in res
        assert res["data"]["healthy"] is True
        assert res["data"]["ready"] is True
        assert "uptimeSeconds" in res["data"]
        assert res["data"]["capture"]["running"] is False
        assert res["data"]["capture"]["state"] == "stopped"

        # Test 2: GET /api/v1/assets
        print("Testing GET /api/v1/assets...")
        status, res = request("/api/v1/assets")
        assert status == 200
        assert res["success"] is True
        assert "assets" in res["data"]
        assert isinstance(res["data"]["assets"], list)

        # Test 3: GET /api/v1/events
        print("Testing GET /api/v1/events...")
        status, res = request("/api/v1/events")
        assert status == 200
        assert res["success"] is True
        assert "events" in res["data"]
        assert isinstance(res["data"]["events"], list)

        # Test 4: GET /api/v1/events?limit=2
        print("Testing GET /api/v1/events?limit=2...")
        status, res = request("/api/v1/events?limit=2")
        assert status == 200
        assert res["success"] is True

        # Test 5: GET /api/v1/events?limit=-5 (validation error)
        print("Testing GET /api/v1/events?limit=-5...")
        status, res = request("/api/v1/events?limit=-5")
        assert status == 400
        assert res["success"] is False
        assert res["error"]["code"] == "invalid_limit"

        # Test 6: GET /api/v1/events?limit=abc (validation error)
        print("Testing GET /api/v1/events?limit=abc...")
        status, res = request("/api/v1/events?limit=abc")
        assert status == 400
        assert res["success"] is False
        assert res["error"]["code"] == "invalid_limit"

        # Test 7: GET /api/v1/logs
        print("Testing GET /api/v1/logs...")
        status, res = request("/api/v1/logs")
        assert status == 200
        assert res["success"] is True
        assert "logs" in res["data"]
        assert isinstance(res["data"]["logs"], list)

        # Test 8: GET /api/v1/logs?limit=-1 (validation error)
        print("Testing GET /api/v1/logs?limit=-1...")
        status, res = request("/api/v1/logs?limit=-1")
        assert status == 400
        assert res["success"] is False
        assert res["error"]["code"] == "invalid_limit"

        # Test 9: GET /api/v1/metrics
        print("Testing GET /api/v1/metrics...")
        status, res = request("/api/v1/metrics")
        assert status == 200
        assert res["success"] is True
        assert "assetCount" in res["data"]
        assert "eventCount" in res["data"]
        assert "captureStarts" in res["data"]
        assert "captureStops" in res["data"]

        # Test 10: POST /api/v1/capture/start
        print("Testing POST /api/v1/capture/start...")
        status, res = request("/api/v1/capture/start", method="POST")
        assert status in (200, 400)
        if status == 200:
            assert res["success"] is True
            assert "state" in res["data"]
        else:
            assert res["success"] is False
            assert res["error"]["code"] == "capture_command_rejected"

        # Test 11: POST /api/v1/capture/stop
        print("Testing POST /api/v1/capture/stop...")
        status, res = request("/api/v1/capture/stop", method="POST")
        assert status in (200, 400)

        # Test 12: POST /api/v1/capture/restart
        print("Testing POST /api/v1/capture/restart...")
        status, res = request("/api/v1/capture/restart", method="POST")
        assert status in (200, 400)

        # Test 13: GET /api/v1/nonexistent (Not Found)
        print("Testing GET /api/v1/nonexistent...")
        status, res = request("/api/v1/nonexistent")
        assert status == 404
        assert res["success"] is False
        assert res["error"]["code"] == "not_found"

        print("All REST API integration tests passed successfully!")

    finally:
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
