#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "esp_wifi.h"
#include "ping/ping_sock.h"
#include "lwip/ip_addr.h"
#include "lwip/inet.h"
#include <math.h>

// Varlik olcumu (Asama 1). Mantik:
//  - Her ~50 ms'de modeme (ag gecidi) bir ping atilir. Gelen cevap cercevesinin CSI'i (her alt tasiyici icin I/Q) okunur.
//  - Alt tasiyici genliklerinin yavas bir ortalamasi (taban) tutulur; her paketin tabandan sapmasi "anlik hareket"tir.
//  - Skor = anlik hareketin kisa sureli ortalamasi. Oda bossa dusuk ve sakin, biri hareket edince yukselir.
// Seri komutlar: wifi <ag> <sifre> | ip <adres> [ag_gecidi] | ip auto | status | reboot
// WiFi sifresi yalnizca cihazin NVS'inde saklanir (koda/depoya yazilmaz).

#define PIR_PIN          4        // PIR sensorun OUT ucu (HC-SR501 / AM312 / HC-SR505)
#define PING_INTERVAL_MS 40
#define BASELINE_ALPHA   0.02f    // genlik tabaninin yavas takibi (yavas ortam degisimlerini sifirlar)
#define SCORE_ALPHA      0.25f    // skor yumusatma
#define MAX_SC           64       // islenecek alt tasiyici sayisi

static Preferences prefs;
static volatile uint8_t g_pir = 0;            // PIR su an hareket goruyor mu
static volatile uint32_t g_pirEvents = 0;     // 0 -> 1 gecis sayisi (hareket olayi)
static volatile uint32_t g_pirLastMs = 0;     // son olayin zamani
static WebServer server(80);
static bool g_haveSsid = false;
static uint32_t g_wifiTry = 0;
static bool g_wasUp = false;

// ---- CSI durumu ----
// Uc olcut yan yana tutulur (hangisi daha ayirt edici oldugunu olcmek icin):
//   old  = anlik sapma (ilk surum; genlik dalgalanmasindan etkilenir)
//   win  = 1 sn'lik pencerede alt tasiyici sekli degisimi (her paket kendi ortalamasina bolunur: guc/AGC dalgalanmasi silinir)
//   corr = 1 - korelasyon(sekil, yavas taban)  (Espressif/ESPectre tarzi)
// Yalnizca MODEMDEN gelen ve 64 alt tasiyicili (LLTF) cerceveler islenir: baska cihazlardan gelen paketler olcumu bozuyordu.
static volatile float g_old = 0, g_win = 0, g_corr = 0, g_rssi = 0;
static volatile uint32_t g_pkts = 0, g_lastPkt = 0, g_skipped = 0;
static volatile float g_rate = 0;
static volatile uint32_t g_cb = 0, g_dbgWarm = 0, g_dbgValid = 0, g_dbgLen = 0;   // teshis sayaclari
static volatile float g_dbgMean = 0;
static uint8_t g_bssid[6] = {0};
static bool g_haveBssid = false;
#define SC 64
#define WIN 20
static float g_base[SC];            // ham genlik tabani (old icin)
static float g_shapeBase[SC];       // sekil tabani (corr icin)
static float g_ring[WIN][SC];       // son WIN paketin normalize genligi
static int g_ringN = 0, g_ringI = 0;
static float g_emaAmp[SC];
static bool g_mask[SC];
static uint32_t g_warm = 0;
static bool g_baseInit = false;
static uint32_t g_rateCount = 0, g_rateAt = 0;

static void csiReset() {
  g_baseInit = false; g_ringN = 0; g_ringI = 0; g_warm = 0;
  for (int k = 0; k < SC; k++) { g_emaAmp[k] = 0; g_mask[k] = true; }
}

static void csiCb(void *, wifi_csi_info_t *info) {
  g_cb++;
  if (!info || !info->buf) return;
  g_dbgLen = info->len;
  if (!g_haveBssid || memcmp(info->mac, g_bssid, 6) != 0) { g_skipped++; return; }   // sadece modemden gelenler
  if (info->len < 2 * SC) { g_skipped++; return; }
  // Modem saniyede yuzlerce cerceve gonderebiliyor; hesap WiFi gorevinde calistigi icin en fazla ~25 paket/sn islenir (yoksa kart donuyor).
  static uint32_t lastProc = 0;
  const uint32_t nowMs = millis();
  if (nowMs - lastProc < 30) return;
  lastProc = nowMs;
  const int8_t *d = info->buf;
  float amp[SC];
  for (int k = 0; k < SC; k++) { const float re = d[2 * k + 1], im = d[2 * k]; amp[k] = sqrtf(re * re + im * im); }

  // gecerli alt tasiyici maskesi: ilk ~60 pakette ortalama genligi dusuk olanlar (koruma bantlari, DC) atilir
  if (g_warm < 60) {
    for (int k = 0; k < SC; k++) g_emaAmp[k] += (amp[k] - g_emaAmp[k]) * (g_warm == 0 ? 1.0f : 0.1f);
    g_warm++; g_dbgWarm = g_warm;
    if (g_warm == 60) {
      float m = 0; for (int k = 0; k < SC; k++) m += g_emaAmp[k]; m /= SC;
      for (int k = 0; k < SC; k++) g_mask[k] = g_emaAmp[k] > 0.4f * m;
    }
    return;
  }
  float mean = 0; int n = 0;
  for (int k = 0; k < SC; k++) if (g_mask[k]) { mean += amp[k]; n++; }
  g_dbgValid = n;
  if (n < 20) return;
  mean /= n;
  g_dbgMean = mean;
  if (mean < 1) return;

  float nrm[SC];
  for (int k = 0; k < SC; k++) nrm[k] = g_mask[k] ? amp[k] / mean : 0;

  if (!g_baseInit) {
    for (int k = 0; k < SC; k++) { g_base[k] = amp[k]; g_shapeBase[k] = nrm[k]; }
    g_baseInit = true;
    return;
  }

  // old: ham genlik sapmasi
  float diff = 0;
  for (int k = 0; k < SC; k++) if (g_mask[k]) { diff += fabsf(amp[k] - g_base[k]); g_base[k] += BASELINE_ALPHA * (amp[k] - g_base[k]); }
  g_old += SCORE_ALPHA * (diff / n / mean - g_old);

  // win: son WIN paketin sekil degisimi (alt tasiyici basina standart sapma ortalamasi)
  for (int k = 0; k < SC; k++) g_ring[g_ringI][k] = nrm[k];
  g_ringI = (g_ringI + 1) % WIN;
  if (g_ringN < WIN) g_ringN++;
  static uint8_t winTick = 0;
  if (g_ringN == WIN && (++winTick % 4) == 0) {   // 4 pakette bir hesapla (CPU)
    float tot = 0;
    for (int k = 0; k < SC; k++) {
      if (!g_mask[k]) continue;
      float mu = 0; for (int i = 0; i < WIN; i++) mu += g_ring[i][k]; mu /= WIN;
      float var = 0; for (int i = 0; i < WIN; i++) { const float e = g_ring[i][k] - mu; var += e * e; }
      tot += sqrtf(var / WIN);
    }
    g_win = tot / n;
  }

  // corr: 1 - Pearson(sekil, yavas sekil tabani)
  float ma = 0, mb = 0; for (int k = 0; k < SC; k++) if (g_mask[k]) { ma += nrm[k]; mb += g_shapeBase[k]; } ma /= n; mb /= n;
  float sab = 0, saa = 0, sbb = 0;
  for (int k = 0; k < SC; k++) if (g_mask[k]) { const float x = nrm[k] - ma, y = g_shapeBase[k] - mb; sab += x * y; saa += x * x; sbb += y * y; }
  const float r = (saa > 0 && sbb > 0) ? sab / sqrtf(saa * sbb) : 1.0f;
  g_corr += 0.2f * ((1.0f - r) - g_corr);
  for (int k = 0; k < SC; k++) if (g_mask[k]) g_shapeBase[k] += 0.01f * (nrm[k] - g_shapeBase[k]);

  g_rssi += 0.2f * (info->rx_ctrl.rssi - g_rssi);
  g_pkts++;
  g_lastPkt = millis();
  g_rateCount++;
  if (millis() - g_rateAt >= 1000) { g_rate = g_rateCount * 1000.0f / (millis() - g_rateAt); g_rateCount = 0; g_rateAt = millis(); }
}

static void csiStart() {
  wifi_csi_config_t cfg = {};
  cfg.lltf_en = true;
  cfg.htltf_en = true;           // 802.11n (HT) cerceveleri HT-LTF ile gelir; kapaliyken atlaniyordu
  cfg.stbc_htltf2_en = true;
  cfg.ltf_merge_en = true;
  cfg.channel_filter_en = false;
  cfg.manu_scale = false;
  esp_wifi_set_csi_config(&cfg);
  esp_wifi_set_csi_rx_cb(csiCb, nullptr);
  esp_wifi_set_csi(true);
}

// ---- ping (modeme) ----
static esp_ping_handle_t g_ping = nullptr;
static volatile uint32_t g_pingOk = 0, g_pingTo = 0;
static void onPingOk(esp_ping_handle_t, void *) { g_pingOk++; }
static void onPingTo(esp_ping_handle_t, void *) { g_pingTo++; }

// Ag gecidi ancak DHCP bitince belli olur (WL_CONNECTED'ten biraz sonra); bu yuzden loop'ta her saniye denenir.
static bool pingStart() {
  if (g_ping) { esp_ping_stop(g_ping); esp_ping_delete_session(g_ping); g_ping = nullptr; }
  const IPAddress gw = WiFi.gatewayIP();
  if (gw == IPAddress(0, 0, 0, 0)) return false;
  esp_ping_config_t cfg = ESP_PING_DEFAULT_CONFIG();
  ip_addr_t t = {};
  t.type = IPADDR_TYPE_V4;
  t.u_addr.ip4.addr = (uint32_t)gw;
  cfg.target_addr = t;
  cfg.count = 0;                                    // sonsuz
  cfg.interval_ms = PING_INTERVAL_MS;
  cfg.timeout_ms = 200;
  cfg.data_size = 1;
  esp_ping_callbacks_t cb = {};
  cb.on_ping_success = onPingOk;
  cb.on_ping_timeout = onPingTo;
  if (esp_ping_new_session(&cfg, &cb, &g_ping) != ESP_OK) { g_ping = nullptr; return false; }
  esp_ping_start(g_ping);
  Serial.printf("[ping] baslatildi: %s, %d ms arayla\n", gw.toString().c_str(), PING_INTERVAL_MS);
  return true;
}

// ---- WiFi ----
static String getStr(const char *k) { return prefs.isKey(k) ? prefs.getString(k, "") : String(); }

static void applyStaticIp() {
  const String ip = getStr("ip");
  if (!ip.length()) { WiFi.config(IPAddress(), IPAddress(), IPAddress()); return; }
  IPAddress a, g;
  if (!a.fromString(ip)) return;
  const String gws = getStr("gw");
  if (!gws.length() || !g.fromString(gws)) g = IPAddress(a[0], a[1], a[2], 1);
  WiFi.config(a, g, IPAddress(255, 255, 255, 0), g);
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

// ---- HTTP ----
static const char PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="tr"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Nowlight varlik olcumu</title><style>
body{margin:0;background:#000;color:#eee;font:16px -apple-system,sans-serif;padding:18px}
h1{font-size:20px;margin:0 0 4px}.s{color:#888;font-size:13px;margin-bottom:14px}
.m{margin:0 0 14px}.l{display:flex;justify-content:space-between;color:#aaa;font-size:13px}.l b{color:#fff;font-size:20px}
.bar{height:22px;background:#222;border-radius:8px;overflow:hidden}.fill{height:100%;background:#FFC93C;width:0;transition:width .15s}
canvas{width:100%;height:70px;background:#0b0b0b;border-radius:8px;margin-top:6px}.r{color:#777;font-size:13px}
</style></head><body><h1>Hareket olcumu</h1><div class="s">Uc olcut yan yana. Her biri kendi olcegine gore cizilir (son 60 sn'nin en yuksegi = tam cubuk).</div>
<div class="m"><div class="l"><span>PIR sensoru</span><b id="pir">-</b></div></div><div id="ms"></div><div class="r" id="r"></div>
<script>
const names={old:"old (ilk yontem)",win:"win (1 sn pencere)",corr:"corr (korelasyon)"};
const keys=Object.keys(names),hist={},mx={};
const ms=document.getElementById('ms');
keys.forEach(k=>{hist[k]=[];mx[k]=1e-6;ms.insertAdjacentHTML('beforeend','<div class="m"><div class="l"><span>'+names[k]+'</span><b id="v_'+k+'">-</b></div><div class="bar"><div class="fill" id="f_'+k+'"></div></div><canvas id="c_'+k+'" width="600" height="70"></canvas></div>')});
async function t(){try{const d=await (await fetch('/api/csi')).json();
keys.forEach(k=>{const v=d[k];hist[k].push(v);if(hist[k].length>300)hist[k].shift();mx[k]=Math.max(1e-6,...hist[k]);
document.getElementById('v_'+k).textContent=v.toFixed(4);document.getElementById('f_'+k).style.width=Math.min(100,v/mx[k]*100)+'%';
const c=document.getElementById('c_'+k),x=c.getContext('2d');x.clearRect(0,0,600,70);x.strokeStyle='#FFC93C';x.beginPath();
hist[k].forEach((y,i)=>{const px=i*2,py=68-(y/mx[k])*64;i?x.lineTo(px,py):x.moveTo(px,py)});x.stroke();});
document.getElementById('pir').textContent=d.pir?'HAREKET':'sakin';document.getElementById('pir').style.color=d.pir?'#FFC93C':'#888';
document.getElementById('r').textContent='paket/sn '+d.rate.toFixed(0)+' | sinyal '+d.rssi.toFixed(0)+' dBm | atlanan '+d.skipped;}catch(e){}}
setInterval(t,200);t();</script></body></html>)HTML";

static void startHttp() {
  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html; charset=utf-8", PAGE); });
  server.on("/api/ping", HTTP_GET, []() { server.send(200, "application/json", "{\"name\":\"nowlight-presence\"}"); });
  server.on("/api/csi", HTTP_GET, []() {
    char b[420];
    snprintf(b, sizeof(b), "{\"old\":%.4f,\"win\":%.4f,\"corr\":%.4f,\"rate\":%.1f,\"rssi\":%.1f,\"pkts\":%u,\"skipped\":%u,\"age\":%lu,\"pingOk\":%u,\"pingTo\":%u,\"cb\":%u,\"warm\":%u,\"valid\":%u,\"len\":%u,\"mean\":%.1f,\"pir\":%u,\"pirEvents\":%u,\"pirAge\":%lu}",
             (float)g_old, (float)g_win, (float)g_corr, (float)g_rate, (float)g_rssi, (unsigned)g_pkts, (unsigned)g_skipped, (unsigned long)(millis() - g_lastPkt), (unsigned)g_pingOk, (unsigned)g_pingTo, (unsigned)g_cb, (unsigned)g_dbgWarm, (unsigned)g_dbgValid, (unsigned)g_dbgLen, (float)g_dbgMean, (unsigned)g_pir, (unsigned)g_pirEvents, (unsigned long)(g_pirEvents ? millis() - g_pirLastMs : 0));
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", b);
  });
  server.begin();
}

// ---- seri komutlar ----
static void handleSerial() {
  static String line;
  while (Serial.available()) {
    const char c = Serial.read();
    if (c != '\n' && c != '\r') { line += c; continue; }
    if (!line.length()) continue;
    if (line.startsWith("wifi ")) {
      String rest = line.substring(5); rest.trim();
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
      ssid = strip(ssid); pass = strip(pass);
      if (!ssid.length()) { Serial.println("[wifi] ag adi bos. Ornek: wifi AgAdim Sifrem"); line = ""; continue; }
      prefs.putString("ssid", ssid); prefs.putString("pass", pass);
      Serial.printf("[wifi] kaydedildi: %s (sifre %d karakter)\n", ssid.c_str(), (int)pass.length());
      wifiConnect();
    } else if (line.startsWith("ip ")) {
      String rest = line.substring(3); rest.trim();
      if (rest == "auto") { prefs.remove("ip"); prefs.remove("gw"); }
      else {
        const int sp = rest.indexOf(' ');
        const String ip = sp < 0 ? rest : rest.substring(0, sp);
        IPAddress t;
        if (!t.fromString(ip)) { Serial.println("[ip] gecersiz adres. Ornek: ip 192.168.1.51"); line = ""; continue; }
        prefs.putString("ip", ip);
        if (sp >= 0) prefs.putString("gw", rest.substring(sp + 1)); else if (prefs.isKey("gw")) prefs.remove("gw");
      }
      Serial.println("[ip] kaydedildi, yeniden baglaniliyor");
      wifiConnect();
    } else if (line == "status") {
      Serial.printf("[durum] wifi=%s ip=%s kanal=%d old=%.3f win=%.3f corr=%.4f paket/sn=%.1f atlanan=%u sinyal=%.0f\n", WiFi.status() == WL_CONNECTED ? "bagli" : "yok",
                    WiFi.localIP().toString().c_str(), (int)WiFi.channel(), (float)g_old, (float)g_win, (float)g_corr, (float)g_rate, (unsigned)g_skipped, (float)g_rssi);
    } else if (line == "reboot") ESP.restart();
    else Serial.println("komutlar: wifi <ag> <sifre> | ip <adres> [ag_gecidi] | ip auto | status | reboot");
    line = "";
  }
}

void setup() {
  Serial.begin(115200);
  delay(400);
  pinMode(PIR_PIN, INPUT_PULLDOWN);              // sensor bagli degilken 0 okunur
  prefs.begin("presence", false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);                // uyku modu paket zamanlamasini bozar
  startHttp();
  wifiConnect();
  Serial.println("\n=== Nowlight varlik olcumu (asama 1) ===");
}

void loop() {
  handleSerial();
  server.handleClient();
  const bool up = WiFi.status() == WL_CONNECTED;
  if (up && !g_wasUp) {
    memcpy(g_bssid, WiFi.BSSID(), 6); g_haveBssid = true;
    csiReset();
    Serial.printf("[wifi] bagli: ip=%s kanal=%d (ESP-NOW kanali 6 olmali)\n", WiFi.localIP().toString().c_str(), (int)WiFi.channel());
    csiStart();
    Serial.printf("[hazir] tarayicidan: http://%s/\n", WiFi.localIP().toString().c_str());
  }
  if (!up && g_wasUp) { Serial.println("[wifi] baglanti koptu"); if (g_ping) { esp_ping_stop(g_ping); esp_ping_delete_session(g_ping); g_ping = nullptr; } }
  static uint32_t lastPingTry = 0;
  if (up && !g_ping && millis() - lastPingTry > 1000) { lastPingTry = millis(); pingStart(); }
  g_wasUp = up;
  if (!up && g_haveSsid && millis() - g_wifiTry > 20000) wifiConnect();

  // PIR: 15 ms'de bir oku, 3 ardisik ayni okumayla degisimi kabul et (parazit filtresi); 0->1 gecisi bir "hareket olayi"
  static uint32_t lastPir = 0; static uint8_t pirSame = 0, pirRaw = 0;
  if (millis() - lastPir >= 15) {
    lastPir = millis();
    const uint8_t r = digitalRead(PIR_PIN);
    if (r == pirRaw) { if (pirSame < 255) pirSame++; } else { pirRaw = r; pirSame = 1; }
    if (pirSame >= 3 && r != g_pir) {
      g_pir = r;
      if (r) { g_pirEvents++; g_pirLastMs = millis(); Serial.printf("[pir] HAREKET (olay %u)\n", (unsigned)g_pirEvents); }
      else Serial.println("[pir] sakin");
    }
  }
  static uint32_t lastPrint = 0;
  if (up && millis() - lastPrint > 500) {
    lastPrint = millis();
    Serial.printf("[csi] pir=%u old=%.3f win=%.3f corr=%.4f paket/sn=%.0f ping ok/zaman asimi=%u/%u sinyal=%.0f\n", (unsigned)g_pir, (float)g_old, (float)g_win, (float)g_corr, (float)g_rate, (unsigned)g_pingOk, (unsigned)g_pingTo, (float)g_rssi);
  }
  delay(1);
}
