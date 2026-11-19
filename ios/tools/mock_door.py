#!/usr/bin/env python3
"""Gelistirme: gercek kapi dugumu (firmware/door) olmadan Otomasyon ekranini denemek icin ayni HTTP arayuzunu taklit eder.
Kullanim: python3 ios/tools/mock_door.py [port] [token]      (varsayilan 8081, token 'test')
Uygulamada Otomasyon > adres 127.0.0.1:8081, token test. Kural metni gercek yazilimdaki gibi dogrulanir (6 alan ya da 7 alan: gunler 1..127).
"""
import json, sys, threading
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import urlparse, parse_qs

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8081
TOKEN = sys.argv[2] if len(sys.argv) > 2 else "test"
rules = ""
lock = threading.Lock()

def valid(text):
    n = 0
    for item in text.split(";"):
        if not item: continue
        v = item.split(":")
        if len(v) not in (6, 7, 8) or any(not f.lstrip("-").isdigit() for f in v): return False
        v = [int(f) for f in v]
        days = v[6] if len(v) >= 7 else 127
        rev = v[7] if len(v) == 8 else 0
        if rev != 0 and not (15 <= rev <= 600): return False
        if not (0 <= v[1] <= 2 and 1 <= v[2] <= 255 and 0 <= v[3] <= 1439 and 0 <= v[4] <= 1439 and 0 <= v[5] <= 60 and 1 <= days <= 127): return False
        n += 1
        if n > 8: return False
    return True

def status():
    return {"enabled": 1, "mode": 1, "lamps": 1, "cooldown": 600, "winOn": 0, "winStart": 1080, "winEnd": 420, "pir": 0, "motions": 3,
            "motionAge": 120, "trigAge": 600, "timeSynced": 1, "time": "09:41", "inWindow": 1, "bridgeSet": 1, "lastResult": "henuz tetiklenmedi",
            "rules": len([r for r in rules.split(";") if r]), "activeRule": -1}

class H(BaseHTTPRequestHandler):
    def _send(self, code, obj):
        b = json.dumps(obj).encode()
        self.send_response(code); self.send_header("Content-Type", "application/json"); self.send_header("Content-Length", str(len(b))); self.end_headers(); self.wfile.write(b)
    def _auth(self):
        if self.headers.get("X-Token") == TOKEN: return True
        self._send(401, {"error": "token"}); return False
    def do_GET(self):
        p = urlparse(self.path).path
        if p == "/api/ping": return self._send(200, {"name": "nowlight-door"})
        if p == "/api/status" and self._auth(): return self._send(200, status())
        if p == "/api/rules" and self._auth(): return self._send(200, {"rules": rules})
        self._send(404, {"error": "yok"})
    def do_POST(self):
        global rules
        u = urlparse(self.path); n = int(self.headers.get("Content-Length", 0))
        q = parse_qs(self.rfile.read(n).decode()) if n else parse_qs(u.query)
        if not self._auth(): return
        if u.path == "/api/rules":
            t = q.get("rules", [""])[0]
            if not valid(t): print("REDDEDILDI:", t, flush=True); return self._send(400, {"error": "kurallar gecersiz"})
            with lock: rules = t
            print("kurallar:", t, flush=True)
            return self._send(200, status())
        if u.path == "/api/config": return self._send(200, status())
        self._send(404, {"error": "yok"})
    def log_message(self, *a): pass

HTTPServer(("127.0.0.1", PORT), H).serve_forever()
