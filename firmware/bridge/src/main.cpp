#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include "esp_random.h"
#include "now_proto.h"
#include "now_link.h"
#include "pairing.h"
#include "improv.h"
#include "ota.h"
#include "schedule.h"
#include <time.h>

// Telefon koprusu: iOS uygulamasi/widget'lari yerel agdan HTTP ile komut verir, kopru ESP-NOW ile dugumlere iletir.
//
//   GET  /api/ping                     kimlik (token gerekmez): {"name":"nowlight-bridge"}
//   GET  /api/state                    {"masa":0|1|-1,"yatak":..,"led":..,"links":{"masa":true,"yatak":false},"channel":1,"wifiChannel":1}
//   POST /api/set   dev=masa|yatak|led|d3..d6|all  val=0|1
//   Eslesme: POST /api/pair/start | GET /api/pair | POST /api/pair/accept?mac=..&id=d3[&pin=5&inv=1] | POST /api/pair/stop
//   Zamanli otomasyon: GET /api/schedules | POST /api/schedules?rules=en:eylem:cihazlar:dakika:gunler:aci;...&tz=180 (bkz. shared/schedule.h)
//   Dugum: POST /api/node/config?id=d3&pin=5&inv=1 | POST /api/node/forget?id=d3 (eslesmeyi siler, kopru yeniden baslar)
//   (durumda eslesen dugumler: "devices":{"d3":1}, links:{"d3":true})
// Token: istek basligi "X-Token". Ilk acilista rastgele uretilir, seri porttan "token" ile gosterilir.
//
// Seri komutlar (115200): wifi <ssid> <sifre> | ip <adres> [ag_gecidi] | ip auto | token | newtoken | status | reboot
// ip: sabit IP (modem ayarina dokunmadan); verilmezse ya da "ip auto" ile DHCP.
// WiFi sifresi ve token yalnizca cihazin NVS'inde saklanir, koda ve depoya yazilmaz.

static const char *DEV_NAMES[3] = {"masa", "yatak", "led"};
static Preferences prefs;
static WebServer server(80);
static String g_token;
static int8_t g_st[3] = {-1, -1, -1};      // son bilinen durum (-1 = henuz bildirilmedi)
#ifndef FW_VERSION
#define FW_VERSION "0.5.0-dev"
#endif
#ifdef NOW_GENERIC
#define NL_CHANNEL ((int)WiFi.channel())          // genel kopru: ESP-NOW kanali router'in kanali
#define NL_LEGACY false
#else
#define NL_CHANNEL NOW_CHANNEL
#define NL_LEGACY true
#endif
#ifndef NL_BOOT_PIN
#define NL_BOOT_PIN 0
#endif
#define SETUP_WINDOW_MS (10UL * 60UL * 1000UL)   // kurulum penceresi: token'i uygulamaya otomatik verir
static uint32_t g_setupUntil = 0;
static uint32_t g_wifiTry = 0;
static bool g_wifiWasUp = false;
static bool g_haveSsid = false;            // kayitli ag var mi (dongude NVS okumamak icin onbellek)

// ---------------------------------------------------------------- ayarlar
static String randomToken() {
  char b[33];
  for (int i = 0; i < 32; i += 8) snprintf(b + i, 9, "%08x", (unsigned)esp_random());
  return String(b);
}

// Anahtar yokken getString NVS hatasi basar (ve dongude seri porti doldurur): once isKey ile bak.
static String getStr(const char *key) { return prefs.isKey(key) ? prefs.getString(key, "") : String(); }

static void loadSettings() {
  prefs.begin("bridge", false);
  g_token = getStr("token");
  if (g_token.length() < 16) { g_token = randomToken(); prefs.putString("token", g_token); }
}

// Kayitli sabit IP'yi uygular: "ip" NVS anahtari = adres, "gw" = ag gecidi. Maske /24 (255.255.255.0), DNS = ag gecidi.
static void applyStaticIp() {
  const String ip = getStr("ip");
  if (!ip.length()) { WiFi.config(IPAddress(), IPAddress(), IPAddress()); return; }   // DHCP'ye don
  IPAddress a, g;
  if (!a.fromString(ip)) { Serial.printf("[ip] gecersiz adres: %s\n", ip.c_str()); return; }
  String gws = getStr("gw");
  if (!gws.length() || !g.fromString(gws)) g = IPAddress(a[0], a[1], a[2], 1);
  WiFi.config(a, g, IPAddress(255, 255, 255, 0), g);
  Serial.printf("[ip] sabit IP: %s  ag gecidi: %s\n", a.toString().c_str(), g.toString().c_str());
}

static void wifiConnect() {
  const String ssid = getStr("ssid");
  g_haveSsid = ssid.length() > 0;
  if (!g_haveSsid) { Serial.println("[wifi] kayitli ag yok. Seri porttan: wifi <ssid> <sifre>"); return; }
  Serial.printf("[wifi] baglaniliyor: %s\n", ssid.c_str());
  applyStaticIp();                        // kayitli sabit IP varsa (yoksa DHCP)
  WiFi.disconnect(false, false);          // sureklenen bir denemeyi kes (yoksa "sta is connecting, cannot set config" hatasi); WiFi acik kalir, ESP-NOW etkilenmez
  delay(150);
  WiFi.begin(ssid.c_str(), getStr("pass").c_str());
  g_wifiTry = millis();
}

static bool setupOpen() { return g_setupUntil && millis() < g_setupUntil; }
static void openSetup(const char *why) {
  g_setupUntil = millis() + SETUP_WINDOW_MS;
  Serial.printf("[kurulum] pencere acildi (%s), 10 dk: uygulama token'i otomatik alabilir\n", why);
}

// Improv: tarayicidan gelen WiFi bilgisi. Baglanirsa kaydeder; baglanamazsa eski ayari geri yukler.
static bool improvConnect(const String &ssid, const String &pass) {
  const String oldS = getStr("ssid"), oldP = getStr("pass");
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  wifiConnect();
  const uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) delay(200);
  if (WiFi.status() == WL_CONNECTED) {
    improv_set_provisioned(true, "http://" + WiFi.localIP().toString() + "/setup");
    openSetup("WiFi tarayicidan girildi");
    return true;
  }
  prefs.putString("ssid", oldS);
  prefs.putString("pass", oldP);
  if (oldS.length()) wifiConnect();
  return false;
}

// ---------------------------------------------------------------- durum
static void pollState() {
  uint8_t st[4];
  if (now_take_state(st)) for (int i = 0; i < 3; i++) if (st[i] != NOW_UNKNOWN) g_st[i] = st[i];
}

static String stateJson() {
  pollState();
  const uint8_t down = now_link_down_mask();
  String o = "{\"masa\":" + String(g_st[0]) + ",\"yatak\":" + String(g_st[1]) + ",\"led\":" + String(g_st[2]);
  o += ",\"devices\":{";
  bool first = true;
  for (uint8_t slot = NOW_DYN_FIRST; slot < NOW_DYN_FIRST + NOW_DYN_MAX; slot++) {
    if (!now_dyn_present(slot)) continue;
    if (!first) o += ',';
    first = false;
    o += "\"d" + String(slot) + "\":" + String(now_dyn_state(slot));
  }
  String lk;
#ifndef NOW_GENERIC
  lk += "\"masa\":" + String((down & 1) ? "false" : "true") + ",\"yatak\":" + String((down & 2) ? "false" : "true");
#endif
  for (uint8_t slot = NOW_DYN_FIRST; slot < NOW_DYN_FIRST + NOW_DYN_MAX; slot++)
    if (now_dyn_present(slot)) {
      if (lk.length()) lk += ',';
      lk += "\"d" + String(slot) + "\":" + String((down & (1 << (2 + slot - NOW_DYN_FIRST))) ? "false" : "true");
    }
  String roles, angles;
  for (uint8_t slot = NOW_DYN_FIRST; slot < NOW_DYN_FIRST + NOW_DYN_MAX; slot++) {
    if (!now_dyn_present(slot)) continue;
    const uint8_t r = now_role(slot);
    if (roles.length()) roles += ',';
    roles += "\"d" + String(slot) + "\":\"" + (r == ROLE_SERVO ? "servo" : r == ROLE_PIR ? "pir" : "light") + "\"";
    if (r == ROLE_SERVO) {
      if (angles.length()) angles += ',';
      angles += "\"d" + String(slot) + "\":{\"angle\":" + String(now_dyn_angle(slot)) + ",\"free\":" + String(now_dyn_free(slot) ? "true" : "false") +
                ",\"moving\":" + String(now_dyn_moving(slot) ? "true" : "false") + ",\"cur\":" + String(now_dyn_cur(slot)) + "}";
    }
  }
  o += "},\"links\":{" + lk + "},\"roles\":{" + roles + "},\"servo\":{" + angles + "},\"legacy\":" + String(NL_LEGACY ? "true" : "false") +
       ",\"channel\":" + String(NL_CHANNEL) + ",\"wifiChannel\":" + String((int)WiFi.channel()) + "}";
  return o;
}

// ---------------------------------------------------------------- zamanli otomasyon
// Kurallar telefonda yazilir, kopruye gonderilir (NVS "sched"). Kopru NTP ile saati alir ve her dakika kurallara bakar:
// su saatte, su gunlerde su cihazlari ac / kapat / cevir / servoyu acaya getir. Kacirilan tetikler (elektrik kesintisi) atlanir.
static bool authorized();
static SchedRule g_sched[SCHED_MAX];
static int g_nSched = 0;
static SchedState g_schedSt;
static int g_tzMin = 180;                    // yerel saat = UTC + bu kadar dakika (telefon gonderir; Turkiye +180)

static void loadSched() {
  prefs.begin("bridge", true);
  g_tzMin = prefs.getInt("tz", 180);
  String w = prefs.isKey("sched") ? prefs.getString("sched", "") : String();
  prefs.end();
  int n = 0;
  if (sched_parse(w.c_str(), g_sched, SCHED_MAX, &n)) g_nSched = n;
}

static bool timeValid() { return time(nullptr) > 1600000000; }

static void localNow(int32_t &epochDay, uint16_t &minute) {
  const int64_t t = (int64_t)time(nullptr) + (int64_t)g_tzMin * 60;
  epochDay = (int32_t)(t >= 0 ? t / 86400 : -((-t + 86399) / 86400));
  minute = (uint16_t)((t - (int64_t)epochDay * 86400) / 60);
}

static String schedJson() {
  char hhmm[8] = "--:--";
  if (timeValid()) { int32_t d; uint16_t m; localNow(d, m); snprintf(hhmm, sizeof(hhmm), "%02u:%02u", m / 60, m % 60); }
  char wire[SCHED_MAX * 28]; sched_to_string(g_sched, g_nSched, wire, sizeof(wire));
  return String("{\"rules\":\"") + wire + "\",\"tz\":" + g_tzMin + ",\"synced\":" + (timeValid() ? "true" : "false") + ",\"time\":\"" + hhmm + "\",\"count\":" + g_nSched + "}";
}

static void runSchedRule(const SchedRule &r) {
  for (uint8_t slot = 0; slot < 8; slot++) {
    if (!((r.devices >> slot) & 1)) continue;
    int8_t cur = -1; bool servo = false;
    if (slot < 3) { if (!NL_LEGACY) continue; cur = g_st[slot]; }
    else {
      if (!now_dyn_present(slot) || now_role(slot) == ROLE_PIR) continue;
      cur = now_dyn_state(slot); servo = now_role(slot) == ROLE_SERVO;
    }
    if (r.action == SCHED_ANGLE) { if (servo) now_send_servo_goto(slot, r.angle, -1); continue; }
    bool on = r.action == SCHED_ON;
    if (r.action == SCHED_TOGGLE) on = cur != 1;
    now_send_set(slot, on);
  }
  Serial.printf("[program] kural calisti: eylem %u cihazlar 0x%02X\n", r.action, r.devices);
}

static void schedLoop() {
  static uint32_t last = 0;
  if (millis() - last < 1000) return;
  last = millis();
  if (!g_nSched || !timeValid()) return;
  int32_t day; uint16_t minute; localNow(day, minute);
  const uint32_t due = sched_poll(g_sched, g_nSched, g_schedSt, day, minute, 0);
  for (int i = 0; i < g_nSched; i++) if ((due >> i) & 1) runSchedRule(g_sched[i]);
}

static void handleSchedulesGet() { if (!authorized()) return; server.send(200, "application/json", schedJson()); }
static void handleSchedulesPost() {
  if (!authorized()) return;
  int n = 0; SchedRule tmp[SCHED_MAX];
  const String w = server.arg("rules");
  if (!sched_parse(w.c_str(), tmp, SCHED_MAX, &n)) { server.send(400, "application/json", "{\"error\":\"kurallar gecersiz\"}"); return; }
  if (server.hasArg("tz")) { const int tz = server.arg("tz").toInt(); if (tz >= -840 && tz <= 840) g_tzMin = tz; }
  for (int i = 0; i < n; i++) g_sched[i] = tmp[i];
  g_nSched = n;
  g_schedSt = SchedState();
  prefs.begin("bridge", false); prefs.putString("sched", w); prefs.putInt("tz", g_tzMin); prefs.end();
  Serial.printf("[program] %d kural kaydedildi (UTC%+d dk)\n", n, g_tzMin);
  server.send(200, "application/json", schedJson());
}

// ---------------------------------------------------------------- HTTP
static bool sameToken(const String &a, const String &b) {     // sabit sureli karsilastirma
  if (a.length() != b.length()) return false;
  uint8_t d = 0;
  for (size_t i = 0; i < a.length(); i++) d |= a[i] ^ b[i];
  return d == 0;
}

static bool authorized() {
  if (sameToken(server.header("X-Token"), g_token)) return true;
  server.send(401, "application/json", "{\"error\":\"token\"}");
  return false;
}

static void handlePing() {
  char b[100];
  snprintf(b, sizeof(b), "{\"name\":\"nowlight-bridge\",\"ip\":\"%s\"}", WiFi.localIP().toString().c_str());
  server.send(200, "application/json", b);
}

static void handleState() {
  if (!authorized()) return;
  now_query();                                    // bir sonraki okuma icin tazele
  server.send(200, "application/json", stateJson());
}

// "masa" | "yatak" | "led" | "d3".."d6" -> yuva numarasi (yoksa -1)
static int devSlot(const String &dev) {
  for (int i = 0; i < 3; i++) if (dev == DEV_NAMES[i]) return i;
  if (dev.length() == 2 && dev[0] == 'd' && isDigit(dev[1])) {
    const int slot = dev[1] - '0';
    if (now_dyn_present(slot)) return slot;
  }
  return -1;
}

static void handleSet() {
  if (!authorized()) return;
  const String dev = server.arg("dev"), val = server.arg("val");
  if (val != "0" && val != "1") { server.send(400, "application/json", "{\"error\":\"val\"}"); return; }
  const bool on = (val == "1");
  if (dev == "all") {
    for (int i = 0; i < 3; i++) now_send_set(i, on);
    for (int s = NOW_DYN_FIRST; s < NOW_DYN_FIRST + NOW_DYN_MAX; s++) if (now_dyn_present(s) && now_role(s) == ROLE_LIGHT) now_send_set(s, on);   // servo ve PIR "tum isiklar"a katilmaz
  } else {
    const int d = devSlot(dev);
    if (d < 0) { server.send(400, "application/json", "{\"error\":\"dev\"}"); return; }
    if (d >= NOW_DYN_FIRST && now_role(d) == ROLE_PIR) { server.send(400, "application/json", "{\"error\":\"rol: PIR sensoru ac/kapat yapilamaz\"}"); return; }
    now_send_set(d, on);
  }
  Serial.printf("[http] set %s=%d\n", dev.c_str(), on);
  server.send(200, "application/json", "{\"ok\":true}");   // gercek onay: sonraki /api/state'te dugumun bildirdigi deger
}

// ---- eslesme
static void handlePairStart() { if (!authorized()) return; pair_start(); server.send(200, "application/json", pair_json()); }
static void handlePairStatus() { if (!authorized()) return; server.send(200, "application/json", pair_json()); }
static void handlePairStop() { if (!authorized()) return; pair_stop(); server.send(200, "application/json", "{\"ok\":true}"); }
static void handlePairAccept() {
  if (!authorized()) return;
  const String id = server.arg("id");
  if (id.length() != 2 || id[0] != 'd' || !isDigit(id[1])) { server.send(400, "application/json", "{\"error\":\"id: yalnizca d3..d6 (yerlesik isiklarin yerine gecme henuz yok)\"}"); return; }
  String err;
  const int pin = server.hasArg("pin") ? server.arg("pin").toInt() : -1;
  const int inv = server.hasArg("inv") ? (server.arg("inv") == "1" ? 1 : 0) : -1;
  const String roleArg = server.arg("role");
  const uint8_t role = roleArg == "servo" ? ROLE_SERVO : roleArg == "pir" ? ROLE_PIR : ROLE_LIGHT;
  if (!pair_accept(server.arg("mac"), id[1] - '0', pin, inv, role, err)) { server.send(400, "application/json", "{\"error\":\"" + err + "\"}"); return; }
  server.send(200, "application/json", "{\"ok\":true}");
}

// ---- eslesen dugumun yonetimi
static uint32_t g_restartAt = 0;

static int customSlot(const String &id) {     // "d3".."d6" -> yuva, yoksa -1
  if (id.length() != 2 || id[0] != 'd' || !isDigit(id[1])) return -1;
  const int s = id[1] - '0';
  return now_dyn_present(s) ? s : -1;
}
// Role ayari icin yuva: "d3".."d7" ya da (kisisel derlemede) "masa" = 0 / "yatak" = 1 (eski hub ve yatak dugumleri artik NOW_CFG anliyor)
static int relaySlot(const String &id) {
  if (NL_LEGACY) { if (id == "masa") return 0; if (id == "yatak") return 1; }
  return customSlot(id);
}
static void handleNodeConfig() {
  if (!authorized()) return;
  const int slot = relaySlot(server.arg("id")), pin = server.arg("pin").toInt();
  if (slot < 0) { server.send(400, "application/json", "{\"error\":\"id\"}"); return; }
  if (!server.hasArg("pin") || pin < 0 || pin > 60) { server.send(400, "application/json", "{\"error\":\"pin\"}"); return; }
  now_send_cfg(slot, pin, server.arg("inv") == "1");
  server.send(200, "application/json", "{\"ok\":true}");
}
// ---- servo ve PIR (rol: eslesmede secilir)
static int roleSlot(const String &id, uint8_t role) {
  const int s = customSlot(id);
  return (s >= 0 && now_role(s) == role) ? s : -1;
}
static int argInt(const char *name, int def, int lo, int hi) {
  if (!server.hasArg(name)) return def;
  const int v = server.arg(name).toInt();
  return v < lo ? lo : v > hi ? hi : v;
}
// POST /api/node/servo?id=d3&pin=5&inv=0&off=0&on=90&speed=0&release=10&min=0&max=180
static void handleServoConfig() {
  if (!authorized()) return;
  const int slot = roleSlot(server.arg("id"), ROLE_SERVO);
  if (slot < 0) { server.send(400, "application/json", "{\"error\":\"id\"}"); return; }
  if (!server.hasArg("pin")) { server.send(400, "application/json", "{\"error\":\"pin\"}"); return; }
  const int pin = argInt("pin", 5, 0, 60), minA = argInt("min", 0, 0, 180), maxA = argInt("max", 180, 0, 180);
  const int offA = argInt("off", 0, 0, 180), onA = argInt("on", 90, 0, 180);
  if (minA > maxA) { server.send(400, "application/json", "{\"error\":\"aci siniri\"}"); return; }
  now_send_servo_cfg(slot, pin, server.arg("inv") == "1", offA, onA, argInt("speed", 0, 0, 255), argInt("release", 10, 0, 255), minA, maxA);
  server.send(200, "application/json", "{\"ok\":true}");
}
// POST /api/servo?id=d3&angle=45[&speed=20]   (elle kontrol: kaydirici)
static void handleServoGoto() {
  if (!authorized()) return;
  const int slot = roleSlot(server.arg("id"), ROLE_SERVO);
  if (slot < 0 || !server.hasArg("angle")) { server.send(400, "application/json", "{\"error\":\"id/angle\"}"); return; }
  now_send_servo_goto(slot, argInt("angle", 0, 0, 180), server.hasArg("speed") ? argInt("speed", 0, 0, 255) : -1);
  server.send(200, "application/json", "{\"ok\":true}");
}
// POST /api/servo/free?id=d3   (servoyu birak: elle oynatilabilir)
static void handleServoFree() {
  if (!authorized()) return;
  const int slot = roleSlot(server.arg("id"), ROLE_SERVO);
  if (slot < 0) { server.send(400, "application/json", "{\"error\":\"id\"}"); return; }
  now_send_servo_free(slot);
  server.send(200, "application/json", "{\"ok\":true}");
}
// POST /api/node/pir?id=d4&pin=4&hold=5&confirm=20&cooldown=0&inv=0
static void handlePirConfig() {
  if (!authorized()) return;
  const int slot = roleSlot(server.arg("id"), ROLE_PIR);
  if (slot < 0 || !server.hasArg("pin")) { server.send(400, "application/json", "{\"error\":\"id/pin\"}"); return; }
  now_send_pir_cfg(slot, argInt("pin", 4, 0, 60), argInt("hold", 5, 0, 255), argInt("confirm", 5, 0, 255), argInt("cooldown", 0, 0, 255), server.arg("inv") == "1");
  server.send(200, "application/json", "{\"ok\":true}");
}
static void handleNodeForget() {
  if (!authorized()) return;
  const int slot = customSlot(server.arg("id"));
  if (slot < 0) { server.send(400, "application/json", "{\"error\":\"id\"}"); return; }
  now_forget(slot);
  g_restartAt = millis() + 1500;               // eski baglanti nesneleri temizlensin: kopru kisaca yeniden baslar
  server.send(200, "application/json", "{\"ok\":true,\"restarting\":true}");
}

static void handleSetupApi() {
  if (!setupOpen()) { server.send(403, "application/json", "{\"error\":\"setup\"}"); return; }
  char b[160];
  snprintf(b, sizeof(b), "{\"name\":\"nowlight-bridge\",\"ip\":\"%s\",\"token\":\"%s\"}", WiFi.localIP().toString().c_str(), g_token.c_str());
  server.send(200, "application/json", b);
}

static void handleSetupPage() {
  String h = "<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'><title>Nowlight koprusu</title>"
             "<style>body{font:17px system-ui;background:#0b0b0c;color:#f2f2f4;max-width:520px;margin:32px auto;padding:0 16px}code{background:#222;padding:3px 8px;border-radius:6px;word-break:break-all}.b{color:#ffc93c}</style>"
             "<h2>Nowlight k&ouml;pr&uuml;s&uuml;</h2>";
  if (setupOpen()) {
    h += "<p>K&ouml;pr&uuml; a&#287;a ba&#287;land&#305;.</p><p>Adres: <code>" + WiFi.localIP().toString() + "</code></p><p>Token: <code>" + g_token + "</code></p>"
         "<p class=b>Uygulamada <b>Cihaz Ekle &rarr; K&ouml;pr&uuml; ayarlar&#305; &rarr; K&ouml;pr&uuml;y&uuml; otomatik bul</b> d&uuml;&#287;mesine bas; adres ve token kendili&#287;inden dolar. Bu sayfa 10 dakika sonra kapan&#305;r.</p>";
  } else {
    h += "<p>Kurulum penceresi kapal&#305;. K&ouml;pr&uuml;n&uuml;n <b>BOOT</b> tu&#351;una 2 saniye bas&#305;l&#305; tut, sonra bu sayfay&#305; yenile.</p>";
  }
  server.send(200, "text/html", h);
}

// ---- kablosuz guncelleme (yalnizca genel kopru)
static void handleVersion() { if (!authorized()) return; server.send(200, "application/json", ota_version_json()); }
static void handleUpdateStatus() { if (!authorized()) return; server.send(200, "application/json", ota_status_json()); }
static void handleUpdateCheck() {
  if (!authorized()) return;
  if (!ota_supported()) { server.send(400, "application/json", "{\"error\":\"ota yok\"}"); return; }
  ota_check();
  server.send(200, "application/json", ota_status_json());
}
static void handleUpdateStart() {
  if (!authorized()) return;
  if (!ota_start()) { server.send(400, "application/json", "{\"error\":\"guncelleme baslatilamadi (yeni surum yok ya da mesgul)\"}"); return; }
  server.send(200, "application/json", ota_status_json());
}

static void startHttp() {
  const char *keys[] = {"X-Token"};
  server.collectHeaders(keys, 1);
  server.on("/api/ping", HTTP_GET, handlePing);
  server.on("/api/setup", HTTP_GET, handleSetupApi);
  server.on("/api/version", HTTP_GET, handleVersion);
  server.on("/api/update", HTTP_GET, handleUpdateStatus);
  server.on("/api/update/check", HTTP_POST, handleUpdateCheck);
  server.on("/api/update/start", HTTP_POST, handleUpdateStart);
  server.on("/setup", HTTP_GET, handleSetupPage);
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/set", HTTP_POST, handleSet);
  server.on("/api/pair/start", HTTP_POST, handlePairStart);
  server.on("/api/pair", HTTP_GET, handlePairStatus);
  server.on("/api/pair/accept", HTTP_POST, handlePairAccept);
  server.on("/api/pair/stop", HTTP_POST, handlePairStop);
  server.on("/api/node/config", HTTP_POST, handleNodeConfig);
  server.on("/api/node/forget", HTTP_POST, handleNodeForget);
  server.on("/api/schedules", HTTP_GET, handleSchedulesGet);
  server.on("/api/schedules", HTTP_POST, handleSchedulesPost);
  server.on("/api/node/servo", HTTP_POST, handleServoConfig);
  server.on("/api/servo", HTTP_POST, handleServoGoto);
  server.on("/api/servo/free", HTTP_POST, handleServoFree);
  server.on("/api/node/pir", HTTP_POST, handlePirConfig);
  server.onNotFound([]() { server.send(404, "application/json", "{\"error\":\"yok\"}"); });
  server.begin();
}

// ---------------------------------------------------------------- seri komutlar
static void printStatus() {
  Serial.printf("[durum] wifi=%s ip=%s kanal(wifi)=%d kanal(espnow)=%d %s\n",
                WiFi.status() == WL_CONNECTED ? "bagli" : "yok", WiFi.localIP().toString().c_str(), (int)WiFi.channel(), NL_CHANNEL,
                (WiFi.status() == WL_CONNECTED && WiFi.channel() != NL_CHANNEL) ? "<< UYARI: router kanali ESP-NOW kanalindan farkli!" : "");
  Serial.printf("[durum] %s\n", stateJson().c_str());
}

static void handleSerial() {   // (satir sonu: \n ya da \r)
  static String line;
  while (Serial.available()) {
    const char c = Serial.read();
    if (line.length() == 0 && improv_feed((uint8_t)c)) continue;       // Improv paketi (tarayicidan WiFi kurulumu)
    if (c != '\n' && c != '\r') { line += c; continue; }
    if (!line.length()) continue;
    if (line.startsWith("wifi ")) {
      // wifi <ag_adi> <sifre>   ya da   wifi "Ag Adi" sifre   (< > ve " isaretleri atilir; ag adinda bosluk varsa tirnak kullan)
      String rest = line.substring(5);
      rest.trim();
      String ssid, pass;
      if (rest.startsWith("\"")) {
        const int q = rest.indexOf('"', 1);
        ssid = q < 0 ? rest.substring(1) : rest.substring(1, q);
        pass = q < 0 ? "" : rest.substring(q + 1);
      } else {
        const int sp = rest.indexOf(' ');
        ssid = sp < 0 ? rest : rest.substring(0, sp);
        pass = sp < 0 ? "" : rest.substring(sp + 1);
      }
      auto strip = [](String s) { s.trim(); if (s.startsWith("<") && s.endsWith(">") && s.length() >= 2) s = s.substring(1, s.length() - 1); if (s.startsWith("\"") && s.endsWith("\"") && s.length() >= 2) s = s.substring(1, s.length() - 1); return s; };
      ssid = strip(ssid);
      pass = strip(pass);
      if (!ssid.length()) { Serial.println("[wifi] ag adi bos. Ornek: wifi AgAdim Sifrem"); line = ""; continue; }
      prefs.putString("ssid", ssid);
      prefs.putString("pass", pass);
      Serial.printf("[wifi] kaydedildi: %s (sifre %d karakter)\n", ssid.c_str(), (int)pass.length());
      wifiConnect();
    } else if (line.startsWith("ip ")) {
      String rest = line.substring(3); rest.trim();
      if (rest == "auto") {
        prefs.remove("ip"); prefs.remove("gw");
        Serial.println("[ip] DHCP'ye donuldu, yeniden baglaniliyor");
      } else {
        const int sp = rest.indexOf(' ');
        const String ip = sp < 0 ? rest : rest.substring(0, sp);
        const String gw = sp < 0 ? "" : rest.substring(sp + 1);
        IPAddress t;
        if (!t.fromString(ip)) { Serial.println("[ip] gecersiz adres. Ornek: ip 192.168.1.50   (ag gecidi 192.168.1.1 varsayilir) ya da: ip auto"); line = ""; continue; }
        prefs.putString("ip", ip);
        if (gw.length()) prefs.putString("gw", gw); else prefs.remove("gw");
        Serial.printf("[ip] kaydedildi: %s, yeniden baglaniliyor\n", ip.c_str());
      }
      wifiConnect();
    } else if (line == "setup") {
      openSetup("seri komut");
    } else if (line == "token") {
      Serial.printf("[token] %s\n", g_token.c_str());
    } else if (line == "newtoken") {
      g_token = randomToken();
      prefs.putString("token", g_token);
      Serial.printf("[token] yeni: %s\n", g_token.c_str());
    } else if (line == "status") {
      printStatus();
    } else if (line == "reboot") {
      ESP.restart();
    } else {
      Serial.println("komutlar: wifi <ssid> <sifre> | ip <adres> [ag_gecidi] | ip auto | token | newtoken | setup | status | reboot");
    }
    line = "";
  }
}

// ---------------------------------------------------------------- ana dongu
void setup() {
  Serial.begin(115200);
  delay(300);
  loadSettings();
  loadSched();
  configTime(0, 0, "pool.ntp.org", "time.google.com");   // saat: WiFi baglaninca kendiliginden alinir
  now_init();                          // ESP-NOW (sabit kanal); WiFi radyosunu da baslatir
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);                // uyku modu ESP-NOW alimini kacirabilir
  startHttp();
  pinMode(NL_BOOT_PIN, INPUT_PULLUP);                                  // BOOT: 2 sn basili = kurulum penceresi
  improv_begin("Nowlight Bridge", FW_VERSION, "ESP32", "Nowlight koprusu", improvConnect);
  wifiConnect();
  Serial.println("\n=== Telefon koprusu ===");
  Serial.printf("[token] %s  (iOS uygulamasina gir; yenilemek icin: newtoken)\n", g_token.c_str());
}

void loop() {
  handleSerial();
  server.handleClient();
  pollState();
  pair_loop();
  schedLoop();
  if (g_restartAt && millis() > g_restartAt) ESP.restart();

  const bool up = WiFi.status() == WL_CONNECTED;
  if (up && !g_wifiWasUp) {
    Serial.printf("[wifi] bagli: ip=%s kanal=%d\n", WiFi.localIP().toString().c_str(), (int)WiFi.channel());
#ifndef NOW_GENERIC
    if (WiFi.channel() != NOW_CHANNEL)
      Serial.printf("[wifi] UYARI: router kanali %d, ESP-NOW kanali %d. Router'in 2.4 GHz kanalini %d'e sabitle, yoksa dugumlerle konusamaz.\n",
                    (int)WiFi.channel(), NOW_CHANNEL, NOW_CHANNEL);
#endif
    if (MDNS.begin("nowlight")) { MDNS.addService("http", "tcp", 80); Serial.println("[mdns] nowlight.local"); }
  }
  if (up && !g_wifiWasUp) improv_set_provisioned(true, "http://" + WiFi.localIP().toString() + "/setup");
  g_wifiWasUp = up;
  { static uint32_t bootDown = 0;                              // BOOT 2 sn basili: kurulum penceresini ac
    if (digitalRead(NL_BOOT_PIN) == LOW) { if (!bootDown) bootDown = millis(); else if (millis() - bootDown > 2000 && !setupOpen()) openSetup("BOOT tusu"); } else bootDown = 0; }
  if (!up && g_haveSsid && millis() - g_wifiTry > 20000) wifiConnect();   // takilirsa yeniden dene
  delay(2);
}
