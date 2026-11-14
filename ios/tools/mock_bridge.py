#!/usr/bin/env python3
"""Gelistirme: gercek kopru (firmware/bridge) olmadan uygulamayi/widget'i denemek icin ayni HTTP arayuzunu taklit eder.
Kullanim: python3 ios/tools/mock_bridge.py [port] [token]      (varsayilan 8080, token 'test')
Uygulamayi simulatorde:  xcrun simctl launch booted online.matestudios.nowlight -bridgeHost 127.0.0.1:8080 -bridgeToken test
"""
import json, sys, threading, time
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import urlparse, parse_qs

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
TOKEN = sys.argv[2] if len(sys.argv) > 2 else "test"
state = {"masa": 1, "yatak": 0, "led": 0}
lock = threading.Lock()

class H(BaseHTTPRequestHandler):
    def _send(self, code, obj):
        b = json.dumps(obj).encode()
        self.send_response(code); self.send_header("Content-Type", "application/json"); self.send_header("Content-Length", str(len(b))); self.end_headers(); self.wfile.write(b)
    def _auth(self):
        if self.headers.get("X-Token") == TOKEN: return True
        self._send(401, {"error": "token"}); return False
    def do_GET(self):
        p = urlparse(self.path).path
        if p == "/api/ping": return self._send(200, {"name": "nowlight-bridge"})
        if p == "/api/state" and self._auth():
            with lock: s = dict(state)
            return self._send(200, {**s, "links": {"masa": True, "yatak": True}, "channel": 1, "wifiChannel": 1})
        if p != "/api/state": self._send(404, {"error": "yok"})
    def do_POST(self):
        u = urlparse(self.path)
        if u.path == "/api/set" and self._auth():
            q = parse_qs(u.query); dev = q.get("dev", [""])[0]; val = q.get("val", [""])[0]
            if val not in ("0", "1"): return self._send(400, {"error": "val"})
            time.sleep(0.15)   # ESP-NOW gecikmesi
            with lock:
                for d in (state if dev == "all" else [dev]):
                    if d in state: state[d] = int(val)
            print("set", dev, val, flush=True)
            return self._send(200, {"ok": True})
        if u.path != "/api/set": self._send(404, {"error": "yok"})
    def log_message(self, *a): pass

HTTPServer(("127.0.0.1", PORT), H).serve_forever()
