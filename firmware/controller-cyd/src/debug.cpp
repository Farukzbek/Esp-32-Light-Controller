#ifdef UI_DEBUG
#include <Arduino.h>
#include <lvgl.h>
#include "hal_cyd.h"
#include "app_ui.h"

// Gelistirme araci (env:cyd-debug): ekrani gormeden dogrulamak icin seri komutlar.
//   tap X Y [ms]  sanal dokunma        snap  ekran goruntusu (RLE + base64, tools/snap.py okur)
//   dev I 0|1     cihaz durumu enjekte  mem   bellek durumu
static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void b64_line(const char *tag, const uint8_t *d, size_t n) {
  char out[160];
  size_t o = 0;
  for (size_t i = 0; i < n; i += 3) {
    const uint32_t v = (d[i] << 16) | ((i + 1 < n ? d[i + 1] : 0) << 8) | (i + 2 < n ? d[i + 2] : 0);
    out[o++] = B64[(v >> 18) & 63];
    out[o++] = B64[(v >> 12) & 63];
    out[o++] = (i + 1 < n) ? B64[(v >> 6) & 63] : '=';
    out[o++] = (i + 2 < n) ? B64[v & 63] : '=';
  }
  out[o] = 0;
  Serial.print(tag);
  Serial.print(' ');
  Serial.println(out);
}

static void snapshot() {
  Serial.printf("SNAPHDR %d %d\n", SCREEN_W, SCREEN_H);
  extern bool g_ui_freeze;
  g_ui_freeze = true;
  hal_freeze(true);
  for (int half = 0; half < 4; half++) {
    const int y0 = half * (SCREEN_H / 4), y1 = y0 + SCREEN_H / 4;
    if (!hal_capture_begin(y0, y1)) { Serial.println("SNAPERR nomem"); g_ui_freeze = false; hal_freeze(false); return; }
    lv_obj_invalidate(lv_scr_act());
    for (int i = 0; i < 30; i++) { lv_timer_handler(); delay(20); }
    hal_capture_end();
    int a, b;
    const uint16_t *d = hal_capture_data(&a, &b);
    const size_t n = (size_t)SCREEN_W * (b - a);
    char tag[3] = {'D', (char)('0' + half), 0};
    uint8_t buf[90];
    size_t bl = 0;
    for (size_t i = 0; i < n;) {                  // RLE: [adet][lo][hi]
      size_t j = i + 1;
      while (j < n && d[j] == d[i] && j - i < 255) j++;
      buf[bl++] = j - i;
      buf[bl++] = d[i] & 0xFF;
      buf[bl++] = d[i] >> 8;
      i = j;
      if (bl >= 87) { b64_line(tag, buf, bl); bl = 0; }
    }
    if (bl) b64_line(tag, buf, bl);
  }
  g_ui_freeze = false;
  hal_freeze(false);
  Serial.println("SNAPEND");
}

void debug_poll(void) {
  static String line;
  while (Serial.available()) {
    const char c = Serial.read();
    if (c != '\n' && c != '\r') { line += c; continue; }
    if (!line.length()) continue;
    int x, y, x2, y2, ms = 150, dev, on;
    if (sscanf(line.c_str(), "tap %d %d %d", &x, &y, &ms) >= 2) { hal_inject_touch(x, y, ms); Serial.printf("OK tap %d %d\n", x, y); }
    else if (sscanf(line.c_str(), "drag %d %d %d %d %d", &x, &y, &x2, &y2, &ms) == 5) { hal_inject_drag(x, y, x2, y2, ms); Serial.printf("OK drag %d %d -> %d %d\n", x, y, x2, y2); }
    else if (line == "snap") snapshot();
    else if (sscanf(line.c_str(), "dev %d %d", &dev, &on) == 2) { ui_debug_set(dev, on); Serial.printf("OK dev %d %d\n", dev, on); }
    else if (line == "mem") Serial.printf("MEM free=%u maxblock=%u\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
    else Serial.printf("? %s\n", line.c_str());
    line = "";
  }
}
#endif
