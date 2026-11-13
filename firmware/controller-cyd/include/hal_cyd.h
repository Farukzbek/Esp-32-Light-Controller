#pragma once
#include <stdint.h>
#include <stdbool.h>

// CYD (ESP32-2432S028R) donanim katmani: ekran (TFT_eSPI, HSPI), dokunmatik (XPT2046, VSPI),
// arka isik (GPIO21, PWM), isik sensoru (LDR, GPIO34) ve LVGL ekran/girdi suruculeri.

#define SCREEN_W 320
#define SCREEN_H 240

void hal_init(void);                         // arka isik, ekran, dokunmatik, lv_init + surucu kaydi
void hal_set_backlight(uint8_t percent);     // 0-100
int  hal_light_percent(void);                // LDR'den: karanlikta dusuk, aydinlikta yuksek (0-100)
void hal_request_calibration(void);          // dokunmatik kalibrasyonu bir sonraki hal_service()'te baslar
void hal_service(void);                      // loop'ta cagir
void hal_set_flip(bool flip);                // ekrani 180 derece cevir (dokunma da buna gore eslenir)
void hal_set_screen_off(bool off);           // kapaliyken ilk dokunus sadece uyandirir (tiklama sayilmaz)
void hal_set_wake_cb(void (*cb)(void));      // kapali ekrana dokunulunca cagrilir

#ifdef UI_DEBUG
void hal_inject_touch(int x, int y, uint32_t ms);          // sanal dokunma
void hal_inject_drag(int x1, int y1, int x2, int y2, uint32_t ms);   // sanal kaydirma
void hal_freeze(bool on);                                  // yakalama sirasinda dokunma yok sayilir
bool hal_capture_begin(int y0, int y1);                    // flush sirasinda satirlari kopyalar
const uint16_t *hal_capture_data(int *y0, int *y1);
void hal_capture_end(void);
#endif
