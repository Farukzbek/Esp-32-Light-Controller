#include <Arduino.h>
#include <lvgl.h>
#include "hal_cyd.h"
#include "panel_cfg.h"
#include "now_link.h"
#include "app_ui.h"

// CYD uzerindeki RGB LED (R=4, G=16, B=17, ters mantik): kapali kalsin
static void rgb_led_off() {
  const int pins[3] = {4, 16, 17};
  for (int p : pins) { pinMode(p, OUTPUT); digitalWrite(p, HIGH); }
}

void setup() {
  Serial.begin(115200);
  rgb_led_off();
  Serial.println("\n=== Kapi paneli (CYD) ===");
  settings_load();
  hal_init();
  ui_init();
  now_init();
}

void loop() {
  lv_timer_handler();
  hal_service();
#ifdef UI_DEBUG
  debug_poll();
#endif
  delay(5);
}
