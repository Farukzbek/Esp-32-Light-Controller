#include <Arduino.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>
#include <Preferences.h>
#include <lvgl.h>
#include "hal_cyd.h"
#include "panel_cfg.h"

// CYD pinleri. Ekran HSPI (platformio.ini bayraklari), dokunmatik ayri VSPI hattinda.
#define PIN_BACKLIGHT 21
#define PIN_LDR       34
#define TOUCH_CLK  25
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CS   33
#define TOUCH_IRQ  36
#define CYD_ROTATION 3     // yatay (bu CYD'de dogru yon); 180 derece cevrik hali: 1

// Dokunmatik kalibrasyonu: ham (rx,ry) -> ekran (x,y) affine donusumu, cihazda 4 noktadan hesaplanip NVS'de saklanir:
//   x = a*rx + b*ry + c ,  y = d*rx + e*ry + f
struct TouchCal { float a, b, c, d, e, f; bool valid; };
static TouchCal s_cal = {0, 0, 0, 0, 0, 0, false};
static bool s_calRequested = false;
static int s_rot = CYD_ROTATION;       // ekranin su anki yonu (3 normal, 1 = 180 derece)
static int s_calRot = CYD_ROTATION;    // kalibrasyonun yapildigi yon

static TFT_eSPI tft;
static SPIClass vspi(VSPI);
static XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

static const uint32_t BUF_PIXELS = SCREEN_W * SCREEN_H / 10;   // 24 satir; iki tampon
static lv_disp_draw_buf_t s_drawBuf;
static lv_color_t *s_buf1 = nullptr, *s_buf2 = nullptr;   // heap'te: statik DRAM WiFi ile tasiyordu

static bool s_screenOff = false;
static bool s_swallow = false;            // uyandiran dokunus birakilana kadar LVGL'e "basili degil" bildir
static void (*s_wakeCb)(void) = nullptr;

#ifdef UI_DEBUG
static uint32_t s_injUntil = 0;
static int s_injX = 0, s_injY = 0;
static uint32_t s_dragStart = 0, s_dragMs = 0;
static int s_dx1, s_dy1, s_dx2, s_dy2;
static uint16_t *s_cap = nullptr;
static int s_capY0 = 0, s_capY1 = 0;      // [y0, y1) araligindaki satirlar yakalanir
static bool s_capOn = false;
static bool s_freeze = false;
#endif

// ---------------------------------------------------------------- ekran
static void flushCb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *px) {
  const uint32_t w = area->x2 - area->x1 + 1;
  const uint32_t h = area->y2 - area->y1 + 1;
#ifdef UI_DEBUG
  if (s_capOn && s_cap) {
    for (int y = area->y1; y <= area->y2; y++) {
      if (y < s_capY0 || y >= s_capY1) continue;
      memcpy(s_cap + (y - s_capY0) * SCREEN_W + area->x1, px + (y - area->y1) * w, w * sizeof(uint16_t));
    }
  }
#endif
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors(reinterpret_cast<uint16_t *>(&px->full), w * h, true);
  tft.endWrite();
  lv_disp_flush_ready(drv);
}

void hal_set_backlight(uint8_t percent) {
  if (percent > 100) percent = 100;
  ledcWrite(PIN_BACKLIGHT, percent * 255 / 100);
}

// CYD'nin LDR'si TERS calisir: isik artinca voltaj duser (aydinlikta ~140-150 mV, karanlikta 300+ mV).
int hal_light_percent(void) {
  const int mv = analogReadMilliVolts(PIN_LDR);
  int p = 100 - (mv - 150) * 88 / 200;   // 150 mV -> %100, 350 mV -> %12
  if (p < 12) p = 12;
  if (p > 100) p = 100;
  return p;
}

// ---------------------------------------------------------------- dokunmatik
static bool readRaw(int &rx, int &ry) {
  if (!touch.tirqTouched() || !touch.touched()) return false;
  TS_Point p = touch.getPoint();
  rx = p.x;
  ry = p.y;
  return true;
}

static bool touchRead(int16_t &x, int16_t &y) {
  int rx, ry;
  if (!readRaw(rx, ry)) return false;
  float fx, fy;
  if (s_cal.valid) {
    fx = s_cal.a * rx + s_cal.b * ry + s_cal.c;
    fy = s_cal.d * rx + s_cal.e * ry + s_cal.f;
    if (s_calRot != s_rot) { fx = (SCREEN_W - 1) - fx; fy = (SCREEN_H - 1) - fy; }   // kalibrasyon baska yonde yapildiysa
  } else {                                    // kalibrasyon yokken kaba varsayilan
    fx = map(rx, 200, 3700, 0, SCREEN_W - 1);
    fy = map(ry, 240, 3800, 0, SCREEN_H - 1);
  }
  x = constrain((int)fx, 0, SCREEN_W - 1);
  y = constrain((int)fy, 0, SCREEN_H - 1);
  return true;
}

static void indevRead(lv_indev_drv_t *, lv_indev_data_t *data) {
  int16_t x = 0, y = 0;
  bool pressed = false;
#ifdef UI_DEBUG
  if (s_freeze) { data->state = LV_INDEV_STATE_REL; return; }
  if (s_injUntil && (int32_t)(s_injUntil - millis()) > 0) { pressed = true; x = s_injX; y = s_injY; }
  else s_injUntil = 0;
  if (s_dragMs) {
    const uint32_t el = millis() - s_dragStart;
    if (el < s_dragMs) { pressed = true; x = s_dx1 + (s_dx2 - s_dx1) * (int)el / (int)s_dragMs; y = s_dy1 + (s_dy2 - s_dy1) * (int)el / (int)s_dragMs; }
    else s_dragMs = 0;
  }
#endif
  if (!pressed) pressed = touchRead(x, y);

  if (pressed && s_screenOff) {                // kapali ekran: sadece uyandir
    s_swallow = true;
    if (s_wakeCb) s_wakeCb();
  }
  if (s_swallow) {                             // uyandiran dokunus birakilana kadar tiklama olarak gitmesin
    if (!pressed) s_swallow = false;
    data->state = LV_INDEV_STATE_REL;
    return;
  }
  if (pressed) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

// ---- kalibrasyon: ekranin dort kosesindeki hedeflere dokun (ASCII, LVGL disinda dogrudan TFT'ye cizer) ----
static bool waitTouchAverage(int &ax, int &ay) {
  int rx, ry;
  while (!readRaw(rx, ry)) delay(10);
  delay(150);
  long sx = 0, sy = 0;
  int n = 0;
  uint32_t t0 = millis();
  while (readRaw(rx, ry) && millis() - t0 < 900) { sx += rx; sy += ry; n++; delay(10); }
  while (readRaw(rx, ry)) delay(10);
  delay(200);
  if (n < 5) return false;
  ax = sx / n;
  ay = sy / n;
  return true;
}

static double det3d(double m00, double m01, double m02, double m10, double m11, double m12, double m20, double m21, double m22) {
  return m00 * (m11 * m22 - m12 * m21) - m01 * (m10 * m22 - m12 * m20) + m02 * (m10 * m21 - m11 * m20);
}

// En kucuk kareler: t = a*rx + b*ry + c  (n nokta; normal denklemler Cramer ile cozulur)
static bool fitAxis(const int *rx, const int *ry, const int *t, int n, float &a, float &b, float &c) {
  double Sxx = 0, Sxy = 0, Sx = 0, Syy = 0, Sy = 0, Sxt = 0, Syt = 0, St = 0;
  for (int i = 0; i < n; i++) {
    Sxx += (double)rx[i] * rx[i]; Sxy += (double)rx[i] * ry[i]; Sx += rx[i];
    Syy += (double)ry[i] * ry[i]; Sy += ry[i];
    Sxt += (double)rx[i] * t[i]; Syt += (double)ry[i] * t[i]; St += t[i];
  }
  const double D = det3d(Sxx, Sxy, Sx, Sxy, Syy, Sy, Sx, Sy, n);
  if (fabs(D) < 1e-3) return false;
  a = det3d(Sxt, Sxy, Sx, Syt, Syy, Sy, St, Sy, n) / D;
  b = det3d(Sxx, Sxt, Sx, Sxy, Syt, Sy, Sx, St, n) / D;
  c = det3d(Sxx, Sxy, Sxt, Sxy, Syy, Syt, Sx, Sy, St) / D;
  return true;
}

#ifdef UI_LANG_EN
#define CAL_TEXT "Touch the target"
#else
#define CAL_TEXT "Hedefe dokun"
#endif

static void calibrate() {
  const int M = 22;
  const int tx[4] = {M, SCREEN_W - 1 - M, M, SCREEN_W - 1 - M};
  const int ty[4] = {M, M, SCREEN_H - 1 - M, SCREEN_H - 1 - M};
  int rx[4], ry[4];
  TouchCal c = {0, 0, 0, 0, 0, 0, false};
  for (int attempt = 0; attempt < 3; attempt++) {
    for (int i = 0; i < 4; i++) {
      while (true) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_WHITE);
        tft.setTextSize(2);
        tft.setCursor(50, 100);
        tft.print(CAL_TEXT);
        tft.setCursor(130, 130);
        tft.printf("%d / 4", i + 1);
        tft.drawCircle(tx[i], ty[i], 12, TFT_WHITE);
        tft.drawFastHLine(tx[i] - 18, ty[i], 37, TFT_WHITE);
        tft.drawFastVLine(tx[i], ty[i] - 18, 37, TFT_WHITE);
        if (waitTouchAverage(rx[i], ry[i])) break;
      }
      Serial.printf("[cal] nokta %d ham=(%d,%d)\n", i + 1, rx[i], ry[i]);
    }
    TouchCal t = {0, 0, 0, 0, 0, 0, true};
    if (!fitAxis(rx, ry, tx, 4, t.a, t.b, t.c) || !fitAxis(rx, ry, ty, 4, t.d, t.e, t.f)) { Serial.println("[cal] HATA: noktalar dejenere"); continue; }
    float worst = 0;                          // en buyuk sapma (piksel)
    for (int i = 0; i < 4; i++) {
      const float ex = t.a * rx[i] + t.b * ry[i] + t.c - tx[i], ey = t.d * rx[i] + t.e * ry[i] + t.f - ty[i];
      worst = fmaxf(worst, sqrtf(ex * ex + ey * ey));
    }
    Serial.printf("[cal] deneme %d: en buyuk sapma %.1f px\n", attempt + 1, worst);
    c = t;
    if (worst <= 12.0f) break;                // yeterince dogru; degilse (yanlis noktaya basildi) tekrarla
  }
  if (!c.valid) { tft.fillScreen(TFT_BLACK); return; }
  s_cal = c;
  s_calRot = s_rot;
  Preferences pr;
  pr.begin("touchcal", false);
  pr.putBool("ok", true);
  pr.putFloat("a", c.a); pr.putFloat("b", c.b); pr.putFloat("c", c.c);
  pr.putFloat("d", c.d); pr.putFloat("e", c.e); pr.putFloat("f", c.f);
  pr.putInt("rot", s_rot);
  pr.end();
  tft.fillScreen(TFT_BLACK);
  if (lv_scr_act()) lv_obj_invalidate(lv_scr_act());
}

static void loadCalibration() {
  Preferences pr;
  pr.begin("touchcal", false);               // yazilabilir ac: ilk acilista alan olusur
  if (pr.getBool("ok", false)) {
    s_calRot = pr.getInt("rot", CYD_ROTATION);
    s_cal.a = pr.getFloat("a"); s_cal.b = pr.getFloat("b"); s_cal.c = pr.getFloat("c");
    s_cal.d = pr.getFloat("d"); s_cal.e = pr.getFloat("e"); s_cal.f = pr.getFloat("f");
    s_cal.valid = true;
  }
  pr.end();
}

void hal_request_calibration(void) { s_calRequested = true; }

void hal_service(void) {
  if (s_calRequested) {
    s_calRequested = false;
    calibrate();
  }
}

void hal_set_flip(bool flip) {
  s_rot = flip ? 1 : CYD_ROTATION;
  tft.setRotation(s_rot);
  tft.fillScreen(TFT_BLACK);
  if (lv_scr_act()) lv_obj_invalidate(lv_scr_act());
}

void hal_set_screen_off(bool off) { s_screenOff = off; }
void hal_set_wake_cb(void (*cb)(void)) { s_wakeCb = cb; }

// ---------------------------------------------------------------- baslatma
void hal_init(void) {
  ledcAttach(PIN_BACKLIGHT, 5000, 8);
  hal_set_backlight(70);
  pinMode(PIN_LDR, INPUT);
  analogSetAttenuation(ADC_11db);

  s_rot = g_set.flip ? 1 : CYD_ROTATION;
  tft.init();
  tft.setRotation(s_rot);
  tft.fillScreen(TFT_BLACK);

  vspi.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  touch.begin(vspi);
  touch.setRotation(1);
  loadCalibration();
  if (!s_cal.valid) calibrate();               // ilk kurulumda zorunlu: dort hedefe dokun

  lv_init();
  s_buf1 = (lv_color_t *)malloc(BUF_PIXELS * sizeof(lv_color_t));
  s_buf2 = (lv_color_t *)malloc(BUF_PIXELS * sizeof(lv_color_t));
  lv_disp_draw_buf_init(&s_drawBuf, s_buf1, s_buf2, BUF_PIXELS);

  static lv_disp_drv_t drv;
  lv_disp_drv_init(&drv);
  drv.hor_res = SCREEN_W;
  drv.ver_res = SCREEN_H;
  drv.flush_cb = flushCb;
  drv.draw_buf = &s_drawBuf;
  lv_disp_drv_register(&drv);

  static lv_indev_drv_t indev;
  lv_indev_drv_init(&indev);
  indev.type = LV_INDEV_TYPE_POINTER;
  indev.read_cb = indevRead;
  lv_indev_drv_register(&indev);
}

#ifdef UI_DEBUG
void hal_inject_touch(int x, int y, uint32_t ms) { s_injX = x; s_injY = y; s_injUntil = millis() + ms; }
void hal_inject_drag(int x1, int y1, int x2, int y2, uint32_t ms) { s_dx1 = x1; s_dy1 = y1; s_dx2 = x2; s_dy2 = y2; s_dragMs = ms; s_dragStart = millis(); }
void hal_freeze(bool on) { s_freeze = on; }
bool hal_capture_begin(int y0, int y1) {
  if (!s_cap) s_cap = (uint16_t *)malloc(SCREEN_W * (SCREEN_H / 4) * sizeof(uint16_t));
  if (!s_cap) return false;
  s_capY0 = y0; s_capY1 = y1; s_capOn = true;
  return true;
}
const uint16_t *hal_capture_data(int *y0, int *y1) { *y0 = s_capY0; *y1 = s_capY1; return s_cap; }
void hal_capture_end(void) { s_capOn = false; }
#endif
