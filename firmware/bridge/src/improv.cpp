#include "improv.h"
#include <WiFi.h>

static const uint8_t HDR[6] = {'I', 'M', 'P', 'R', 'O', 'V'};
enum : uint8_t { T_STATE = 0x01, T_ERROR = 0x02, T_RPC = 0x03, T_RESULT = 0x04 };
enum : uint8_t { S_AUTHORIZED = 0x02, S_PROVISIONING = 0x03, S_PROVISIONED = 0x04 };
enum : uint8_t { E_NONE = 0x00, E_INVALID = 0x01, E_UNKNOWN_CMD = 0x02, E_CONNECT = 0x03 };
enum : uint8_t { C_WIFI = 0x01, C_STATE = 0x02, C_INFO = 0x03, C_SCAN = 0x04, C_IDENTIFY = 0x05 };

static uint8_t s_buf[300];
static int s_pos = 0;
static uint8_t s_state = S_AUTHORIZED;
static String s_url;
static ImprovConnectFn s_connect = nullptr;
static const char *s_fw = "", *s_ver = "", *s_chip = "", *s_name = "";

static void sendPacket(uint8_t type, const uint8_t *data, uint8_t len) {
  uint8_t head[9];
  memcpy(head, HDR, 6); head[6] = 0x01; head[7] = type; head[8] = len;
  uint8_t sum = 0;
  for (int i = 0; i < 9; i++) sum += head[i];
  for (int i = 0; i < len; i++) sum += data[i];
  Serial.write(head, 9);
  if (len) Serial.write(data, len);
  Serial.write(sum);
  Serial.flush();
}
static void sendState() { sendPacket(T_STATE, &s_state, 1); }
static void sendError(uint8_t e) { sendPacket(T_ERROR, &e, 1); }

// RPC sonucu: komut | veri uzunlugu | { uzunluk, bayt... } listesi
static void sendResult(uint8_t cmd, const String *strs, int n) {
  uint8_t data[250];
  int p = 2;
  for (int i = 0; i < n; i++) {
    const int l = min((int)strs[i].length(), 100);
    if (p + 1 + l > 250) break;
    data[p++] = l;
    memcpy(data + p, strs[i].c_str(), l);
    p += l;
  }
  data[0] = cmd; data[1] = p - 2;
  sendPacket(T_RESULT, data, p);
}

static void handleRpc(const uint8_t *d, uint8_t len) {
  if (len < 2) { sendError(E_INVALID); return; }
  const uint8_t cmd = d[0], dlen = d[1];
  if (dlen + 2 > len) { sendError(E_INVALID); return; }
  const uint8_t *p = d + 2;

  switch (cmd) {
    case C_WIFI: {
      if (dlen < 2) { sendError(E_INVALID); return; }
      const uint8_t sl = p[0];
      if (1 + sl + 1 > dlen) { sendError(E_INVALID); return; }
      const uint8_t pl = p[1 + sl];
      if (1 + sl + 1 + pl > dlen) { sendError(E_INVALID); return; }
      String ssid, pass;
      for (int i = 0; i < sl; i++) ssid += (char)p[1 + i];
      for (int i = 0; i < pl; i++) pass += (char)p[2 + sl + i];
      s_state = S_PROVISIONING; sendState();
      const bool ok = s_connect && s_connect(ssid, pass);
      if (ok) {
        s_state = S_PROVISIONED; sendState();
        const String r[1] = {s_url};
        sendResult(C_WIFI, r, s_url.length() ? 1 : 0);
      } else {
        s_state = S_AUTHORIZED;
        sendError(E_CONNECT);
        sendState();
      }
      break;
    }
    case C_STATE: {
      sendState();
      if (s_state == S_PROVISIONED && s_url.length()) { const String r[1] = {s_url}; sendResult(C_STATE, r, 1); }
      break;
    }
    case C_INFO: {
      const String r[4] = {String(s_fw), String(s_ver), String(s_chip), String(s_name)};
      sendResult(C_INFO, r, 4);
      break;
    }
    case C_SCAN: {
      const int n = WiFi.scanNetworks();
      for (int i = 0; i < min(n, 15); i++) {
        const String r[3] = {WiFi.SSID(i), String(WiFi.RSSI(i)), WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "NO" : "YES"};
        sendResult(C_SCAN, r, 3);
      }
      sendResult(C_SCAN, nullptr, 0);                 // liste sonu
      WiFi.scanDelete();
      break;
    }
    case C_IDENTIFY:
      sendResult(C_IDENTIFY, nullptr, 0);
      break;
    default:
      sendError(E_UNKNOWN_CMD);
  }
}

void improv_begin(const char *fw, const char *ver, const char *chip, const char *name, ImprovConnectFn connect) {
  s_fw = fw; s_ver = ver; s_chip = chip; s_name = name; s_connect = connect;
}

void improv_set_provisioned(bool provisioned, const String &url) {
  s_state = provisioned ? S_PROVISIONED : S_AUTHORIZED;
  s_url = url;
}

bool improv_feed(uint8_t b) {
  if (s_pos < 6) {                                    // baslik: IMPROV
    if (b == HDR[s_pos]) { s_buf[s_pos++] = b; return true; }
    s_pos = 0;
    return false;                                     // 'I' ile baslayan ama IMPROV olmayan metin
  }
  s_buf[s_pos++] = b;
  if (s_pos < 9) return true;                         // surum, tur, uzunluk
  const uint8_t len = s_buf[8];
  if (s_pos < 9 + len + 1) return true;               // veri + toplam-kontrol
  uint8_t sum = 0;
  for (int i = 0; i < 9 + len; i++) sum += s_buf[i];
  const bool ok = sum == s_buf[9 + len] && s_buf[6] == 0x01;
  const uint8_t type = s_buf[7];
  uint8_t data[256];
  memcpy(data, s_buf + 9, len);
  s_pos = 0;
  if (ok && type == T_RPC) handleRpc(data, len);
  else if (!ok) sendError(E_INVALID);
  return true;
}
