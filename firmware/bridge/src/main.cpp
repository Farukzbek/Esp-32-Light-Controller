#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include "esp_random.h"
#include "now_proto.h"
#include "now_link.h"

// Telefon koprusu: iOS uygulamasi/widget'lari yerel agdan HTTP ile komut verir, kopru ESP-NOW ile dugumlere iletir.
//
//   GET  /api/ping                     kimlik (token gerekmez): {"name":"nowlight-bridge"}
//   GET  /api/state                    {"masa":0|1|-1,"yatak":..,"led":..,"links":{"masa":true,"yatak":false},"channel":1,"wifiChannel":1}
//   POST /api/set   dev=masa|yatak|led|all  val=0|1
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

// ---------------------------------------------------------------- durum
static void pollState() {
  uint8_t st[4];
  if (now_take_state(st)) for (int i = 0; i < 3; i++) if (st[i] != NOW_UNKNOWN) g_st[i] = st[i];
}

static String stateJson() {
  pollState();
  const uint8_t down = now_link_down_mask();
  char b[200];
  snprintf(b, sizeof(b),
           "{\"masa\":%d,\"yatak\":%d,\"led\":%d,\"links\":{\"masa\":%s,\"yatak\":%s},\"channel\":%d,\"wifiChannel\":%d}",
           g_st[0], g_st[1], g_st[2], (down & 1) ? "false" : "true", (down & 2) ? "false" : "true", NOW_CHANNEL, (int)WiFi.channel());
  return String(b);
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

static void handleSet() {
  if (!authorized()) return;
  const String dev = server.arg("dev"), val = server.arg("val");
  if (val != "0" && val != "1") { server.send(400, "application/json", "{\"error\":\"val\"}"); return; }
  const bool on = (val == "1");
  if (dev == "all") {
    for (int i = 0; i < 3; i++) now_send_set(i, on);
  } else {
    int d = -1;
    for (int i = 0; i < 3; i++) if (dev == DEV_NAMES[i]) d = i;
    if (d < 0) { server.send(400, "application/json", "{\"error\":\"dev\"}"); return; }
    now_send_set(d, on);
  }
  Serial.printf("[http] set %s=%d\n", dev.c_str(), on);
  server.send(200, "application/json", "{\"ok\":true}");   // gercek onay: sonraki /api/state'te dugumun bildirdigi deger
}

static void startHttp() {
  const char *keys[] = {"X-Token"};
  server.collectHeaders(keys, 1);
  server.on("/api/ping", HTTP_GET, handlePing);
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/set", HTTP_POST, handleSet);
  server.onNotFound([]() { server.send(404, "application/json", "{\"error\":\"yok\"}"); });
  server.begin();
}

// ---------------------------------------------------------------- seri komutlar
static void printStatus() {
  Serial.printf("[durum] wifi=%s ip=%s kanal(wifi)=%d kanal(espnow)=%d %s\n",
                WiFi.status() == WL_CONNECTED ? "bagli" : "yok", WiFi.localIP().toString().c_str(), (int)WiFi.channel(), NOW_CHANNEL,
                (WiFi.status() == WL_CONNECTED && WiFi.channel() != NOW_CHANNEL) ? "<< UYARI: router kanali ESP-NOW kanalindan farkli!" : "");
  Serial.printf("[durum] %s\n", stateJson().c_str());
}

static void handleSerial() {   // (satir sonu: \n ya da \r)
  static String line;
  while (Serial.available()) {
    const char c = Serial.read();
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
      Serial.println("komutlar: wifi <ssid> <sifre> | ip <adres> [ag_gecidi] | ip auto | token | newtoken | status | reboot");
    }
    line = "";
  }
}

// ---------------------------------------------------------------- ana dongu
void setup() {
  Serial.begin(115200);
  delay(300);
  loadSettings();
  now_init();                          // ESP-NOW (sabit kanal); WiFi radyosunu da baslatir
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);                // uyku modu ESP-NOW alimini kacirabilir
  startHttp();
  wifiConnect();
  Serial.println("\n=== Telefon koprusu ===");
  Serial.printf("[token] %s  (iOS uygulamasina gir; yenilemek icin: newtoken)\n", g_token.c_str());
}

void loop() {
  handleSerial();
  server.handleClient();
  pollState();

  const bool up = WiFi.status() == WL_CONNECTED;
  if (up && !g_wifiWasUp) {
    Serial.printf("[wifi] bagli: ip=%s kanal=%d\n", WiFi.localIP().toString().c_str(), (int)WiFi.channel());
    if (WiFi.channel() != NOW_CHANNEL)
      Serial.printf("[wifi] UYARI: router kanali %d, ESP-NOW kanali %d. Router'in 2.4 GHz kanalini %d'e sabitle, yoksa dugumlerle konusamaz.\n",
                    (int)WiFi.channel(), NOW_CHANNEL, NOW_CHANNEL);
    if (MDNS.begin("nowlight")) { MDNS.addService("http", "tcp", 80); Serial.println("[mdns] nowlight.local"); }
  }
  g_wifiWasUp = up;
  if (!up && g_haveSsid && millis() - g_wifiTry > 20000) wifiConnect();   // takilirsa yeniden dene
  delay(2);
}
