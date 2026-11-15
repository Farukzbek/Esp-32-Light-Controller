#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <time.h>
#include "esp_random.h"

// Kapi dugumu: PIR sensoru odaya giris/hareket algilayinca KOPRUYE (firmware/bridge) komut atar.
//  Modlar: 0 = cevir (secili isiklarin durumunu tersine cevirir), 1 = yalnizca ac, 2 = yalnizca kapat.
//  Kosullar: etkin mi, bekleme suresi (0 = her harekette), saat araligi (NTP ile; saat alinamazsa aralik acikken tetiklemez).
// Ayarlar uygulamadan HTTP ile degisir (X-Token). Seri komutlar: wifi <ag> <sifre> | ip <adres> | token | newtoken |
//   bridge <adres> <kopru_token> | status | test | reboot.   Sifreler/anahtarlar yalnizca cihazin NVS'inde saklanir.

#define PIR_PIN 4
#define TZ_TURKEY "<+03>-3"

static Preferences prefs;
static WebServer server(80);

struct Cfg {
  bool enabled = false;       // varsayilan KAPALI: ayarlar yapilmadan isik yakmasin
  uint8_t mode = 1;           // 0 cevir, 1 yalnizca ac, 2 yalnizca kapat
  uint8_t lamps = 1;          // bit0 masa, bit1 yatak, bit2 led
  uint16_t cooldownSec = 600; // 0..3600
  bool winOn = false;
  uint16_t winStart = 18 * 60, winEnd = 7 * 60;   // dakika (gece yarisini asabilir)
} g;

static String g_token, g_bridgeHost, g_bridgeToken;
static bool g_haveSsid = false, g_wasUp = false;
static uint32_t g_wifiTry = 0;

// ---- durum ----
static volatile uint8_t g_pir = 0;
static uint32_t g_motions = 0, g_lastMotionMs = 0, g_lastTrigMs = 0;
static String g_lastResult = "henuz tetiklenmedi";
static bool g_timeSynced = false;

// ---------------------------------------------------------------- yardimcilar
static String getStr(const char *k) { return prefs.isKey(k) ? prefs.getString(k, "") : String(); }

static String randomToken() {
  char b[33];
  for (int i = 0; i < 32; i += 8) snprintf(b + i, 9, "%08x", (unsigned)esp_random());
  return String(b);
}

static void loadCfg() {
  g.enabled = prefs.getBool("en", g.enabled);
  g.mode = prefs.getUChar("mode", g.mode); if (g.mode > 2) g.mode = 1;
  g.lamps = prefs.getUChar("lamps", g.lamps) & 7; if (!g.lamps) g.lamps = 1;
  g.cooldownSec = min<uint16_t>(prefs.getUShort("cool", g.cooldownSec), 3600);
  g.winOn = prefs.getBool("winOn", g.winOn);
  g.winStart = min<uint16_t>(prefs.getUShort("winS", g.winStart), 1439);
  g.winEnd = min<uint16_t>(prefs.getUShort("winE", g.winEnd), 1439);
  g_bridgeHost = getStr("bhost");
  g_bridgeToken = getStr("btoken");
  g_token = getStr("dtoken");
  if (g_token.length() < 16) { g_token = randomToken(); prefs.putString("dtoken", g_token); }
}

static void saveCfg() {
  prefs.putBool("en", g.enabled); prefs.putUChar("mode", g.mode); prefs.putUChar("lamps", g.lamps);
  prefs.putUShort("cool", g.cooldownSec); prefs.putBool("winOn", g.winOn);
  prefs.putUShort("winS", g.winStart); prefs.putUShort("winE", g.winEnd);
}

static bool nowLocal(struct tm &t) {
  if (!getLocalTime(&t, 0)) return false;
  return t.tm_year > 120;                         // 2020 sonrasi = saat alindi
}

static bool inWindow() {
  if (!g.winOn) return true;
  struct tm t;
  if (!nowLocal(t)) return false;                 // saat bilinmiyorsa aralik acikken tetikleme
  const uint16_t m = t.tm_hour * 60 + t.tm_min;
  if (g.winStart == g.winEnd) return true;
  return g.winStart < g.winEnd ? (m >= g.winStart && m < g.winEnd) : (m >= g.winStart || m < g.winEnd);
}

// ---------------------------------------------------------------- kopru
static const char *LAMP_NAMES[3] = {"masa", "yatak", "led"};

static int jget(const String &js, const char *key) {   // {"masa":1,...} -> 1 (yoksa -2)
  const String k = String("\"") + key + "\":";
  const int i = js.indexOf(k);
  if (i < 0) return -2;
  return js.substring(i + k.length()).toInt();
}

static bool bridgeReq(const String &path, bool post, String *out) {
  if (!g_bridgeHost.length() || !g_bridgeToken.length()) { g_lastResult = "hata: kopru adresi/anahtari girilmemis"; return false; }
  HTTPClient http;
  http.setConnectTimeout(1200);
  http.setTimeout(1800);
  if (!http.begin("http://" + g_bridgeHost + path)) { g_lastResult = "hata: kopru adresi gecersiz"; return false; }
  http.addHeader("X-Token", g_bridgeToken);
  const int code = post ? http.POST("") : http.GET();
  if (out && code == 200) *out = http.getString();
  http.end();
  if (code != 200) { g_lastResult = "hata: kopru cevap " + String(code); return false; }
  return true;
}

static void act(bool force) {
  String state;
  int cur[3] = {-1, -1, -1};
  if (g.mode == 0) {                              // cevir: mevcut durum lazim
    if (!bridgeReq("/api/state", false, &state)) return;
    for (int i = 0; i < 3; i++) cur[i] = jget(state, LAMP_NAMES[i]);
  }
  String done;
  for (int i = 0; i < 3; i++) {
    if (!(g.lamps & (1 << i))) continue;
    int target = g.mode == 1 ? 1 : g.mode == 2 ? 0 : (cur[i] == 1 ? 0 : 1);
    if (!bridgeReq(String("/api/set?dev=") + LAMP_NAMES[i] + "&val=" + target, true, nullptr)) return;
    done += String(LAMP_NAMES[i]) + "=" + target + " ";
  }
  g_lastResult = String(force ? "test: " : "ok: ") + done;
  Serial.printf("[kural] %s\n", g_lastResult.c_str());
}

static void onMotion() {
  g_motions++;
  g_lastMotionMs = millis();
  if (!g.enabled) { g_lastResult = "hareket gorundu, otomasyon kapali"; return; }
  if (g.cooldownSec && g_lastTrigMs && millis() - g_lastTrigMs < (uint32_t)g.cooldownSec * 1000UL) { g_lastResult = "hareket gorundu, bekleme suresinde"; return; }
  if (!inWindow()) { g_lastResult = "hareket gorundu, saat araligi disinda"; return; }
  g_lastTrigMs = millis();
  act(false);
}

// ---------------------------------------------------------------- HTTP
static bool authorized() {
  const String t = server.header("X-Token");
  if (t.length() == g_token.length()) {
    uint8_t d = 0;
    for (size_t i = 0; i < t.length(); i++) d |= t[i] ^ g_token[i];
    if (d == 0) return true;
  }
  server.send(401, "application/json", "{\"error\":\"token\"}");
  return false;
}

static String statusJson() {
  struct tm t; const bool ok = nowLocal(t);
  char hhmm[8] = "--:--";
  if (ok) snprintf(hhmm, sizeof(hhmm), "%02d:%02d", t.tm_hour, t.tm_min);
  String lr = g_lastResult; lr.replace("\"", "'");
  char b[520];
  snprintf(b, sizeof(b),
           "{\"enabled\":%d,\"mode\":%d,\"lamps\":%d,\"cooldown\":%u,\"winOn\":%d,\"winStart\":%u,\"winEnd\":%u,"
           "\"pir\":%u,\"motions\":%u,\"motionAge\":%ld,\"trigAge\":%ld,\"timeSynced\":%d,\"time\":\"%s\",\"inWindow\":%d,"
           "\"bridgeSet\":%d,\"lastResult\":\"%s\"}",
           g.enabled, g.mode, g.lamps, g.cooldownSec, g.winOn, g.winStart, g.winEnd,
           (unsigned)g_pir, (unsigned)g_motions, g_motions ? (long)((millis() - g_lastMotionMs) / 1000) : -1L,
           g_lastTrigMs ? (long)((millis() - g_lastTrigMs) / 1000) : -1L, ok, hhmm, inWindow(),
           (g_bridgeHost.length() && g_bridgeToken.length()) ? 1 : 0, lr.c_str());
  return String(b);
}

static void handleConfig() {
  if (!authorized()) return;
  if (server.hasArg("enabled")) g.enabled = server.arg("enabled") == "1";
  if (server.hasArg("mode")) { const int v = server.arg("mode").toInt(); if (v >= 0 && v <= 2) g.mode = v; }
  if (server.hasArg("lamps")) { const int v = server.arg("lamps").toInt() & 7; if (v) g.lamps = v; }
  if (server.hasArg("cooldown")) g.cooldownSec = constrain(server.arg("cooldown").toInt(), 0, 3600);
  if (server.hasArg("winOn")) g.winOn = server.arg("winOn") == "1";
  if (server.hasArg("winStart")) g.winStart = constrain(server.arg("winStart").toInt(), 0, 1439);
  if (server.hasArg("winEnd")) g.winEnd = constrain(server.arg("winEnd").toInt(), 0, 1439);
  saveCfg();
  if (server.hasArg("bridgeHost")) { g_bridgeHost = server.arg("bridgeHost"); g_bridgeHost.trim(); prefs.putString("bhost", g_bridgeHost); }
  if (server.hasArg("bridgeToken") && server.arg("bridgeToken").length() >= 8) { g_bridgeToken = server.arg("bridgeToken"); g_bridgeToken.trim(); prefs.putString("btoken", g_bridgeToken); }
  server.send(200, "application/json", statusJson());
}

static void startHttp() {
  const char *keys[] = {"X-Token"};
  server.collectHeaders(keys, 1);
  server.on("/api/ping", HTTP_GET, []() {
    char b[100]; snprintf(b, sizeof(b), "{\"name\":\"nowlight-door\",\"ip\":\"%s\"}", WiFi.localIP().toString().c_str());
    server.send(200, "application/json", b);
  });
  server.on("/api/status", HTTP_GET, []() { if (authorized()) server.send(200, "application/json", statusJson()); });
  server.on("/api/config", HTTP_POST, handleConfig);
  server.on("/api/test", HTTP_POST, []() { if (!authorized()) return; act(true); server.send(200, "application/json", statusJson()); });
  server.onNotFound([]() { server.send(404, "application/json", "{\"error\":\"yok\"}"); });
  server.begin();
}

// ---------------------------------------------------------------- WiFi / seri
static void applyStaticIp() {
  const String ip = getStr("ip");
  if (!ip.length()) { WiFi.config(IPAddress(), IPAddress(), IPAddress()); return; }
  IPAddress a, gw;
  if (!a.fromString(ip)) return;
  const String gws = getStr("gw");
  if (!gws.length() || !gw.fromString(gws)) gw = IPAddress(a[0], a[1], a[2], 1);
  WiFi.config(a, gw, IPAddress(255, 255, 255, 0), gw);
}

static void wifiConnect() {
  const String ssid = getStr("ssid");
  g_haveSsid = ssid.length() > 0;
  if (!g_haveSsid) { Serial.println("[wifi] kayitli ag yok. Seri porttan: wifi <ag> <sifre>"); return; }
  applyStaticIp();
  WiFi.disconnect(false, false);
  delay(150);
  Serial.printf("[wifi] baglaniliyor: %s\n", ssid.c_str());
  WiFi.begin(ssid.c_str(), getStr("pass").c_str());
  g_wifiTry = millis();
}

static String strip(String s) {
  s.trim();
  if (s.startsWith("<") && s.endsWith(">") && s.length() >= 2) s = s.substring(1, s.length() - 1);
  if (s.startsWith("\"") && s.endsWith("\"") && s.length() >= 2) s = s.substring(1, s.length() - 1);
  return s;
}

static void handleSerial() {
  static String line;
  while (Serial.available()) {
    const char c = Serial.read();
    if (c != '\n' && c != '\r') { line += c; continue; }
    if (!line.length()) continue;
    if (line.startsWith("wifi ")) {
      String rest = line.substring(5); rest.trim();
      String ssid, pass;
      if (rest.startsWith("\"")) { const int q = rest.indexOf('"', 1); ssid = q < 0 ? rest.substring(1) : rest.substring(1, q); pass = q < 0 ? "" : rest.substring(q + 1); }
      else { const int sp = rest.indexOf(' '); ssid = sp < 0 ? rest : rest.substring(0, sp); pass = sp < 0 ? "" : rest.substring(sp + 1); }
      ssid = strip(ssid); pass = strip(pass);
      if (!ssid.length()) { Serial.println("[wifi] ag adi bos. Ornek: wifi AgAdim Sifrem"); line = ""; continue; }
      prefs.putString("ssid", ssid); prefs.putString("pass", pass);
      Serial.printf("[wifi] kaydedildi: %s (sifre %d karakter)\n", ssid.c_str(), (int)pass.length());
      wifiConnect();
    } else if (line.startsWith("ip ")) {
      String rest = line.substring(3); rest.trim();
      if (rest == "auto") { prefs.remove("ip"); prefs.remove("gw"); }
      else { const int sp = rest.indexOf(' '); const String ip = sp < 0 ? rest : rest.substring(0, sp); IPAddress t;
        if (!t.fromString(ip)) { Serial.println("[ip] gecersiz adres. Ornek: ip 192.168.1.51"); line = ""; continue; }
        prefs.putString("ip", ip); if (sp >= 0) prefs.putString("gw", rest.substring(sp + 1)); else if (prefs.isKey("gw")) prefs.remove("gw"); }
      Serial.println("[ip] kaydedildi, yeniden baglaniliyor");
      wifiConnect();
    } else if (line.startsWith("bridge ")) {
      String rest = line.substring(7); rest.trim();
      const int sp = rest.indexOf(' ');
      if (sp < 0) { Serial.println("kullanim: bridge <adres> <kopru_token>"); line = ""; continue; }
      g_bridgeHost = strip(rest.substring(0, sp)); g_bridgeToken = strip(rest.substring(sp + 1));
      prefs.putString("bhost", g_bridgeHost); prefs.putString("btoken", g_bridgeToken);
      Serial.printf("[kopru] kaydedildi: %s (anahtar %d karakter)\n", g_bridgeHost.c_str(), (int)g_bridgeToken.length());
    } else if (line == "token") {
      Serial.printf("[token] %s\n", g_token.c_str());
    } else if (line == "newtoken") {
      g_token = randomToken(); prefs.putString("dtoken", g_token); Serial.printf("[token] yeni: %s\n", g_token.c_str());
    } else if (line == "test") {
      act(true);
    } else if (line == "status") {
      Serial.printf("[durum] wifi=%s ip=%s pir=%u hareket=%u sonuc=%s\n", WiFi.status() == WL_CONNECTED ? "bagli" : "yok",
                    WiFi.localIP().toString().c_str(), (unsigned)g_pir, (unsigned)g_motions, g_lastResult.c_str());
      Serial.println(statusJson());
    } else if (line == "reboot") ESP.restart();
    else Serial.println("komutlar: wifi <ag> <sifre> | ip <adres> | token | newtoken | bridge <adres> <kopru_token> | test | status | reboot");
    line = "";
  }
}

void setup() {
  Serial.begin(115200);
  delay(400);
  pinMode(PIR_PIN, INPUT_PULLDOWN);
  prefs.begin("presence", false);                 // eski deney yazilimiyla ayni alan: kayitli WiFi bilgisi korunur
  loadCfg();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  startHttp();
  wifiConnect();
  configTzTime(TZ_TURKEY, "pool.ntp.org", "time.google.com");
  Serial.println("\n=== Nowlight kapi dugumu ===");
  Serial.printf("[ayar] etkin=%d mod=%d lambalar=%d bekleme=%us saat=%d (%u-%u dk)\n", g.enabled, g.mode, g.lamps, g.cooldownSec, g.winOn, g.winStart, g.winEnd);
}

void loop() {
  handleSerial();
  server.handleClient();
  const bool up = WiFi.status() == WL_CONNECTED;
  if (up && !g_wasUp) Serial.printf("[wifi] bagli: ip=%s kanal=%d\n", WiFi.localIP().toString().c_str(), (int)WiFi.channel());
  if (!up && g_wasUp) Serial.println("[wifi] baglanti koptu");
  g_wasUp = up;
  if (!up && g_haveSsid && millis() - g_wifiTry > 20000) wifiConnect();

  // PIR: 15 ms'de bir oku, 3 ardisik ayni okumayla degisimi kabul et; 0->1 gecisi hareket olayi
  static uint32_t lastPir = 0; static uint8_t same = 0, raw = 0;
  if (millis() - lastPir >= 15) {
    lastPir = millis();
    const uint8_t r = digitalRead(PIR_PIN);
    if (r == raw) { if (same < 255) same++; } else { raw = r; same = 1; }
    if (same >= 3 && r != g_pir) {
      g_pir = r;
      if (r) { Serial.println("[pir] HAREKET"); onMotion(); }
      else Serial.println("[pir] sakin");
    }
  }
  static bool timeLogged = false;
  if (!timeLogged) { struct tm t; if (nowLocal(t)) { timeLogged = true; g_timeSynced = true; Serial.printf("[saat] %02d:%02d (NTP)\n", t.tm_hour, t.tm_min); } }
  delay(1);
}
