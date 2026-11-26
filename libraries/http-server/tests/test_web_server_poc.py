#!/usr/bin/env python3
"""Comprehensive verification test for Loom Web Server PoC."""

import http.client
import subprocess
import time
import sys
from pathlib import Path

def run_poc_test():
    repo_root = Path("/Users/lokesh/Desktop/Loom")
    loomc = repo_root / "build" / "loomc"
    lib_rt = repo_root / "build" / "libloom_rt.a"
    stdlib = repo_root / "stdlib"
    server_dir = Path("/Users/lokesh/Desktop/loom-libraries/libraries/http-server")

    poc_src = server_dir / "examples" / "web_server_poc.loom"
    poc_obj = "/tmp/web_server_poc.o"
    poc_exe = "/tmp/web_server_poc"

    print("[BUILD] Compiling web_server_poc.loom...")
    compile_cmd = [
        str(loomc),
        "-I", str(stdlib),
        "-I", str(server_dir / "src"),
        "--object", str(poc_src), poc_obj,
        "--entry"
    ]
    res = subprocess.run(compile_cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"Compilation failed:\n{res.stderr}\n{res.stdout}")
        sys.exit(1)

    print("[BUILD] Linking web_server_poc...")
    link_cmd = ["clang", poc_obj, str(lib_rt), "-o", poc_exe]
    res = subprocess.run(link_cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"Linking failed:\n{res.stderr}\n{res.stdout}")
        sys.exit(1)

    print("[SERVER] Starting Loom Web Server PoC on http://127.0.0.1:8088...")
    proc = subprocess.Popen([poc_exe], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    time.sleep(0.5)

    host = "127.0.0.1"
    port = 8088

    try:
        # 1. Test GET /
        print("[TEST 1/6] GET / (Interactive HTML UI)")
        conn = http.client.HTTPConnection(host, port, timeout=5)
        conn.request("GET", "/")
        res = conn.getresponse()
        body = res.read().decode("utf-8")
        conn.close()

        assert res.status == 200, f"Expected 200, got {res.status}"
        assert "text/html" in res.getheader("Content-Type", "")
        assert "Loom Web Server PoC" in body
        print(f"  -> SUCCESS: Status 200 OK, HTML payload received ({len(body)} bytes)")

        # 2. Test GET /api/status
        print("[TEST 2/6] GET /api/status (JSON Service Metadata)")
        conn = http.client.HTTPConnection(host, port, timeout=5)
        conn.request("GET", "/api/status")
        res = conn.getresponse()
        body = res.read().decode("utf-8")
        conn.close()

        assert res.status == 200, f"Expected 200, got {res.status}"
        assert "application/json" in res.getheader("Content-Type", "")
        assert '"status":"online"' in body
        assert '"language":"Loom"' in body
        print(f"  -> SUCCESS: Status 200 OK, Metadata: {body}")

        # 3. Test GET /healthz
        print("[TEST 3/6] GET /healthz (Text Health Probe)")
        conn = http.client.HTTPConnection(host, port, timeout=5)
        conn.request("GET", "/healthz")
        res = conn.getresponse()
        body = res.read().decode("utf-8")
        conn.close()

        assert res.status == 200, f"Expected 200, got {res.status}"
        assert "OK" in body
        print(f"  -> SUCCESS: Status 200 OK, Probe: {body.strip()}")

        # 4. Test POST /api/echo
        print("[TEST 4/6] POST /api/echo (JSON Payload Echo)")
        conn = http.client.HTTPConnection(host, port, timeout=5)
        payload = '{"author":"Loom Team","message":"Concurrency and high performance"}'
        headers = {"Content-Type": "application/json"}
        conn.request("POST", "/api/echo", body=payload, headers=headers)
        res = conn.getresponse()
        body = res.read().decode("utf-8")
        conn.close()

        assert res.status == 200, f"Expected 200, got {res.status}"
        assert "application/json" in res.getheader("Content-Type", "")
        assert "Concurrency and high performance" in body
        print(f"  -> SUCCESS: Status 200 OK, Echoed body: {body}")

        # 5. Test 404 Not Found
        print("[TEST 5/6] GET /nonexistent (404 Fallback)")
        conn = http.client.HTTPConnection(host, port, timeout=5)
        conn.request("GET", "/nonexistent")
        res = conn.getresponse()
        body = res.read().decode("utf-8")
        conn.close()

        assert res.status == 404, f"Expected 404, got {res.status}"
        assert '"error":"Not Found"' in body
        print(f"  -> SUCCESS: Status 404 Not Found, Error response: {body}")

        # 6. Test GET /api/shutdown
        print("[TEST 6/6] GET /api/shutdown (Graceful Shutdown)")
        conn = http.client.HTTPConnection(host, port, timeout=5)
        conn.request("GET", "/api/shutdown")
        res = conn.getresponse()
        body = res.read().decode("utf-8")
        conn.close()

        assert res.status == 200, f"Expected 200, got {res.status}"
        assert '"shutdown":true' in body
        print(f"  -> SUCCESS: Status 200 OK, Shutdown message: {body}")

        proc.wait(timeout=5)
        print(f"[SERVER] Process exited cleanly with code {proc.returncode}")
        print("\nAll 6/6 verification tests passed successfully!")

    finally:
        if proc.poll() is None:
            proc.terminate()
            proc.wait(timeout=2)

if __name__ == "__main__":
    run_poc_test()
