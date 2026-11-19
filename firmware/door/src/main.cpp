#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <time.h>
#include "esp_random.h"
#include "rule_days.h"
#include "reenter.h"

// Kapi dugumu: PIR sensoru odaya giris/hareket algilayinca KOPRUYE (firmware/bridge) komut atar.
//  KURALLAR (en fazla 8, uygulamadan gelir): her kural = etkin mi, eylem (0 cevir, 1 ac, 2 kapat), isiklar (bit maskesi),
//  saat araligi (baslangic dahil, bitis haric; gece yarisini asabilir), bekleme (dk, 0 = her harekette).
//  Harekette saati uyan ILK kural calisir (bekleme suresindeyse hicbir sey yapilmaz). Saat alinamazsa kural calismaz.
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

struct Rule { bool en = true; uint8_t action = 1, lamps = 1; uint16_t start = 0, end = 0, cool = 0; uint8_t days = RULE_ALL_DAYS; uint16_t rev = 0; };   // rev: hizli gir-cik penceresi (sn, 0 = kapali), shared/reenter.h   // days: bit0 Pzt ... bit6 Paz (shared/rule_days.h)
#define MAX_RULES 8
static Rule g_rules[MAX_RULES];
static uint8_t g_nRules = 0;
static uint32_t g_ruleTrig[MAX_RULES];
static ReenterState g_re;         // hizli gir-cik: son calisan kural ve zamani
#define REV_LOCK_MAX_MS 10000     // ... ama toplam en fazla bu kadar (kapinin disinda oyalanan kisi girisi geciktirmesin)
#define REV_LOCK_MS 5000          // ters eylemden sonra sensor bu kadar sessiz kalana kadar hareketler yok sayilir (her hareket sureyi uzatir)
#define REV_MIN_GAP_S 8           // pencere acildiktan en az bu kadar sn sonra gelen hareket "cikis" sayilir (ayni ziyaretteki hareketler degil)
static int g_active = -1;        // son harekette saati uyan kuralin sirasi (-1: yok)

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

static String rulesToString() {
  String o;
  for (int i = 0; i < g_nRules; i++) {
    const Rule &r = g_rules[i];
    if (i) o += ';';
    o += String(r.en ? 1 : 0) + ":" + r.action + ":" + r.lamps + ":" + r.start + ":" + r.end + ":" + r.cool;
    if (r.days != RULE_ALL_DAYS || r.rev) o += ":" + String(r.days);       // 7. alan: gunler (yalnizca gerekliyse, eski uygulamalarla uyum)
    if (r.rev) o += ":" + String(r.rev);                                  // 8. alan: hizli gir-cik penceresi (sn)
  }
  return o;
}

// "en:eylem:isiklar:bas:bit:bekleme[:gunler[:gircik_sn]];..." -> kurallar (gunler: bit0 Pzt ... bit6 Paz, yoksa her gun). Gecersizse hicbir sey degismez.
static bool parseRules(const String &str) {
  Rule tmp[MAX_RULES]; int n = 0;
  int pos = 0;
  while (pos < (int)str.length() && n < MAX_RULES) {
    int e = str.indexOf(';', pos); if (e < 0) e = str.length();
    const String item = str.substring(pos, e);
    pos = e + 1;
    if (!item.length()) continue;
    long v[8]; int k = 0, q = 0;
    while (k < 8) {
      int c = item.indexOf(':', q); const String f = item.substring(q, c < 0 ? item.length() : c);
      if (!f.length()) return false;
      v[k++] = f.toInt();
      if (c < 0) break; q = c + 1;
    }
    if (k == 6) v[6] = RULE_ALL_DAYS;
    if (k < 8) v[7] = 0;                                    // 8. alan yoksa gir-cik kapali
    if ((k != 6 && k != 7 && k != 8) || v[7] < 0 || v[7] > 600 || (v[7] > 0 && v[7] < 15) || v[6] < 1 || v[6] > 127 || v[1] < 0 || v[1] > 2 || v[2] < 1 || v[2] > 255 || v[3] < 0 || v[3] > 1439 || v[4] < 0 || v[4] > 1439 || v[5] < 0 || v[5] > 60) return false;
    tmp[n++] = {v[0] != 0, (uint8_t)v[1], (uint8_t)v[2], (uint16_t)v[3], (uint16_t)v[4], (uint16_t)v[5], (uint8_t)v[6], (uint16_t)v[7]};
  }
  for (int i = 0; i < n; i++) g_rules[i] = tmp[i];
  g_nRules = n;
  memset(g_ruleTrig, 0, sizeof(g_ruleTrig)); reenter_clear(g_re);
  return true;
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
  if (prefs.isKey("rules")) parseRules(getStr("rules"));
  else {                                          // eski tek-kural ayarlarindan tasi
    g_rules[0] = {true, g.mode, g.lamps, g.winOn ? g.winStart : (uint16_t)0, g.winOn ? g.winEnd : (uint16_t)0, (uint16_t)(g.cooldownSec / 60)};
    g_nRules = 1; prefs.putString("rules", rulesToString());
  }
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

static bool inWindowR(const Rule &r) {
  if (r.start == r.end && r.days == RULE_ALL_DAYS) return true;   // gun boyu, her gun: saate gerek yok
  struct tm t;
  if (!nowLocal(t)) return false;                 // saat bilinmiyorsa (gun/saat siniri olan) kural calismaz
  return rule_in_window(r.days, t.tm_wday, t.tm_hour * 60 + t.tm_min, r.start, r.end);
}

// ---------------------------------------------------------------- kopru
// isik kimlikleri = uygulamadaki yuva numarasi: masa 0, yatak 1, led 2, eklenenler d3...d7 (bit = 1 << yuva)
static const char *LAMP_NAMES[8] = {"masa", "yatak", "led", "d3", "d4", "d5", "d6", "d7"};

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

static void act(const Rule &r, bool force, const char *tag = nullptr) {
  String state;
  int cur[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
  if (r.action == 0) {                            // cevir: mevcut durum lazim
    if (!bridgeReq("/api/state", false, &state)) return;
    for (int i = 0; i < 8; i++) cur[i] = jget(state, LAMP_NAMES[i]);
  }
  String done;
  for (int i = 0; i < 8; i++) {
    if (!(r.lamps & (1 << i))) continue;
    int target = r.action == 1 ? 1 : r.action == 2 ? 0 : (cur[i] == 1 ? 0 : 1);
    if (!bridgeReq(String("/api/set?dev=") + LAMP_NAMES[i] + "&val=" + target, true, nullptr)) return;
    done += String(LAMP_NAMES[i]) + "=" + target + " ";
  }
  g_lastResult = String(force ? "test: " : tag ? tag : "ok: ") + done;
  Serial.printf("[kural] %s\n", g_lastResult.c_str());
}

// Kuralin cihazlarindan en az biri su an ACIK mi? (kopruden sorar). Isik manuel acilmis olsa da gir-cik "cikis" sayilsin diye.
static bool anyLampOn(const Rule &r) {
  String state;
  if (!bridgeReq("/api/state", false, &state)) return false;
  for (int i = 0; i < 8; i++) if ((r.lamps & (1 << i)) && jget(state, LAMP_NAMES[i]) == 1) return true;
  return false;
}

static void onMotion() {
  g_motions++;
  g_lastMotionMs = millis();
  if (!g.enabled) { g_lastResult = "hareket gorundu, otomasyon kapali"; return; }
  if (!g_nRules) { g_lastResult = "hareket gorundu, kural yok"; return; }
  // Ters eylemden sonraki sakinlesme: sensor cikan kisiyi gormeye devam eder; bu hareketler yeni giris sayilmaz (isik tekrar yanmasin)
  if (reenter_locked(g_re, millis(), REV_LOCK_MS, REV_LOCK_MAX_MS)) { g_lastResult = "gir-cik sonrasi: sensor sakinlesene kadar hareketler yok sayiliyor"; return; }
  // Hizli gir-cik: son calisan kuralin penceresi aciksa ve bu hareket "cikis" gibi geliyorsa kuralin TERSINI uygula (kural hala saatindeyse)
  if (g_re.armed && g_re.rule >= 0 && g_re.rule < g_nRules) {
    const Rule &pr = g_rules[g_re.rule];
    const ReenterResult rr = reenter_check(g_re, millis(), pr.rev, REV_MIN_GAP_S);
    if (rr == RE_REVERSE && pr.en && g.enabled && inWindowR(pr)) {
      Rule rev = pr; rev.action = reenter_opposite(pr.action);
      const int idx = g_re.rule;
      reenter_clear(g_re);
      reenter_lock(g_re, millis(), REV_LOCK_MS);
      g_ruleTrig[idx] = 0;                              // sonraki gercek giris kurali bekleme suresine takilmasin
      g_lastTrigMs = millis();
      act(rev, false, "gir-cik, ters: ");
      g_lastResult = "kural " + String(idx + 1) + " " + g_lastResult;
      return;
    }
    if (rr == RE_SAME_VISIT) { g_lastResult = "ayni ziyaret (gir-cik penceresi acik)"; return; }
  }
  for (int i = 0; i < g_nRules; i++) {
    const Rule &r = g_rules[i];
    if (!r.en || !inWindowR(r)) continue;
    g_active = i;
    if (r.cool && g_ruleTrig[i] && millis() - g_ruleTrig[i] < (uint32_t)r.cool * 60000UL) {
      g_lastResult = "kural " + String(i + 1) + ": bekleme suresinde"; return;
    }
    // Gir-cik acik "ac" kurali: cihazlar ZATEN aciksa (kural yakmis olsun ya da telefondan acilmis) bu hareket CIKIS sayilir: kapat
    if (r.rev && r.action == 1 && anyLampOn(r)) {
      Rule off = r; off.action = 2;
      reenter_clear(g_re);
      reenter_lock(g_re, millis(), REV_LOCK_MS);
      g_ruleTrig[i] = 0;
      g_lastTrigMs = millis();
      act(off, false, "cikis (isik acikti), kapat: ");
      g_lastResult = "kural " + String(i + 1) + " " + g_lastResult;
      return;
    }
    g_ruleTrig[i] = millis(); if (!g_ruleTrig[i]) g_ruleTrig[i] = 1;
    g_lastTrigMs = millis();
    act(r, false);
    if (r.rev) reenter_arm(g_re, i, millis()); else reenter_clear(g_re);
    g_lastResult = "kural " + String(i + 1) + " " + g_lastResult;
    return;
  }
  g_active = -1;
  g_lastResult = "hareket gorundu, saati uyan kural yok";
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
  int active = -1;
  for (int i = 0; i < g_nRules; i++) if (g_rules[i].en && inWindowR(g_rules[i])) { active = i; break; }
  String lr = g_lastResult; lr.replace("\"", "'");
  char b[420];
  snprintf(b, sizeof(b),
           "{\"enabled\":%d,\"pir\":%u,\"motions\":%u,\"motionAge\":%ld,\"trigAge\":%ld,\"timeSynced\":%d,\"time\":\"%s\","
           "\"bridgeSet\":%d,\"rules\":%u,\"activeRule\":%d,\"lastResult\":\"%s\"}",
           g.enabled, (unsigned)g_pir, (unsigned)g_motions, g_motions ? (long)((millis() - g_lastMotionMs) / 1000) : -1L,
           g_lastTrigMs ? (long)((millis() - g_lastTrigMs) / 1000) : -1L, ok, hhmm,
           (g_bridgeHost.length() && g_bridgeToken.length()) ? 1 : 0, (unsigned)g_nRules, active, lr.c_str());
  return String(b);
}

static void handleConfig() {
  if (!authorized()) return;
  if (server.hasArg("enabled")) g.enabled = server.arg("enabled") == "1";
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
  server.on("/api/rules", HTTP_GET, []() {
    if (!authorized()) return;
    server.send(200, "application/json", "{\"rules\":\"" + rulesToString() + "\"}");
  });
  server.on("/api/rules", HTTP_POST, []() {
    if (!authorized()) return;
    if (!server.hasArg("rules") || !parseRules(server.arg("rules"))) { server.send(400, "application/json", "{\"error\":\"kurallar gecersiz\"}"); return; }
    prefs.putString("rules", rulesToString());
    server.send(200, "application/json", statusJson());
  });
  server.on("/api/test", HTTP_POST, []() {
    if (!authorized()) return;
    const int i = server.hasArg("rule") ? server.arg("rule").toInt() : 0;
    if (i >= 0 && i < g_nRules) act(g_rules[i], true);
    else g_lastResult = "hata: boyle bir kural yok";
    server.send(200, "application/json", statusJson());
  });
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
      if (g_nRules) act(g_rules[0], true);
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
  Serial.printf("[ayar] etkin=%d kural=%u: %s\n", g.enabled, (unsigned)g_nRules, rulesToString().c_str());
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
