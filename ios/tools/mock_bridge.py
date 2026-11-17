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
# Gelistirme: rol deneme icin --roles ile servo (d3) ve PIR (d4) ekle: python3 mock_bridge.py 8080 test --roles
roles = {}
servo = {}      # id -> {"angle", "free", "off", "on"}
if "--roles" in sys.argv:
    state["d3"] = 0; state["d4"] = 0
    roles = {"d3": "servo", "d4": "pir"}
    servo["d3"] = {"angle": 0, "free": True, "off": 0, "on": 90}
pair = {"until": 0.0, "started": 0.0}   # eslesme penceresi (gelistirme: 4 sn sonra sahte bir cihaz bulunur)
def pair_status():
    now = time.time(); is_open = now < pair["until"]
    found = [{"mac": "AA:BB:CC:11:22:33", "name": "Nowlight-1122", "rssi": -52, "chip": 3, "pin": 5, "inv": 1}] if pair["started"] and now - pair["started"] > 4 else []
    return {"open": is_open, "secondsLeft": max(0, int(pair["until"] - now)), "found": found}
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
            legacy = {k: s[k] for k in ("masa", "yatak", "led")}
            others = {k: v for k, v in s.items() if k not in legacy}
            allroles = {k: roles.get(k, "light") for k in others}
            with lock: sv = {k: {"angle": v["angle"], "free": v["free"]} for k, v in servo.items()}
            return self._send(200, {**legacy, "devices": others, "links": {k: True for k in s}, "roles": allroles, "servo": sv, "channel": 1, "wifiChannel": 1})
        if p == "/api/pair" and self._auth(): return self._send(200, pair_status())
        if p not in ("/api/state", "/api/pair"): self._send(404, {"error": "yok"})
    def do_POST(self):
        u = urlparse(self.path)
        if u.path == "/api/set" and self._auth():
            q = parse_qs(u.query); dev = q.get("dev", [""])[0]; val = q.get("val", [""])[0]
            if val not in ("0", "1"): return self._send(400, {"error": "val"})
            time.sleep(0.15)   # ESP-NOW gecikmesi
            with lock:
                for d in (state if dev == "all" else [dev]):
                    if dev == "all" and roles.get(d, "light") != "light": continue      # servo ve PIR "tum isiklar"a katilmaz
                    if d in state:
                        state[d] = int(val)
                        if d in servo: servo[d]["angle"] = servo[d]["on"] if int(val) else servo[d]["off"]; servo[d]["free"] = False
            print("set", dev, val, flush=True)
            return self._send(200, {"ok": True})
        if u.path == "/api/pair/start" and self._auth():
            pair["started"] = time.time(); pair["until"] = time.time() + 120
            return self._send(200, pair_status())
        if u.path == "/api/pair/stop" and self._auth():
            pair["until"] = 0; pair["started"] = 0
            return self._send(200, {"ok": True})
        if u.path == "/api/pair/accept" and self._auth():
            q = parse_qs(u.query); print("accept", q.get("mac"), q.get("id"), "pin", q.get("pin"), "inv", q.get("inv"), "rol", q.get("role"), flush=True)
            with lock:
                i = q.get("id", [""])[0]; state.setdefault(i, 0); r = q.get("role", ["light"])[0]
                if r != "light": roles[i] = r
                if r == "servo": servo[i] = {"angle": 0, "free": True, "off": 0, "on": 90}
            return self._send(200, {"ok": True})
        if u.path == "/api/node/config" and self._auth():
            q = parse_qs(u.query); print("config", q.get("id"), "pin", q.get("pin"), "inv", q.get("inv"), flush=True)
            return self._send(200, {"ok": True})
        if u.path == "/api/node/forget" and self._auth():
            q = parse_qs(u.query); print("forget", q.get("id"), flush=True)
            with lock: state.pop(q.get("id", [""])[0], None)
            return self._send(200, {"ok": True, "restarting": True})
        if u.path == "/api/servo" and self._auth():
            q = parse_qs(u.query); d = q.get("id", [""])[0]; a = int(q.get("angle", ["0"])[0])
            with lock:
                if d in servo:
                    servo[d]["angle"] = a; servo[d]["free"] = False; state[d] = 1 if abs(a - servo[d]["on"]) <= abs(a - servo[d]["off"]) else 0
            print("servo", d, a, flush=True)
            return self._send(200, {"ok": True})
        if u.path == "/api/servo/free" and self._auth():
            d = parse_qs(u.query).get("id", [""])[0]
            with lock:
                if d in servo: servo[d]["free"] = True
            print("servo birak", d, flush=True)
            return self._send(200, {"ok": True})
        if u.path == "/api/node/servo" and self._auth():
            q = parse_qs(u.query); d = q.get("id", [""])[0]
            with lock:
                if d in servo: servo[d]["off"] = int(q.get("off", ["0"])[0]); servo[d]["on"] = int(q.get("on", ["90"])[0])
            print("servo ayar", {k: v[0] for k, v in q.items()}, flush=True)
            return self._send(200, {"ok": True})
        if u.path == "/api/node/pir" and self._auth():
            print("pir ayar", {k: v[0] for k, v in parse_qs(u.query).items()}, flush=True)
            return self._send(200, {"ok": True})
        if u.path not in ("/api/servo", "/api/servo/free", "/api/node/servo", "/api/node/pir", "/api/set", "/api/pair/start", "/api/pair/stop", "/api/pair/accept", "/api/node/config", "/api/node/forget"): self._send(404, {"error": "yok"})
    def log_message(self, *a): pass

HTTPServer(("127.0.0.1", PORT), H).serve_forever()
