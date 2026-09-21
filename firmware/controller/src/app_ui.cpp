#include "lcd_bsp.h"
#include "lcd_config.h"
#include "imu.h"
#include "battery.h"
#include "now_link.h"
#include <Preferences.h>
#include "esp_sleep.h"
#include "sleep_ctl.h"
#include <math.h>

// Ekran sabit fontla Turkce karakter gostermedigi icin etiketler ASCII.
// Dokununca simdilik sadece ekrandaki durum degisiyor; ESP-NOW sonraki adim.

// ---- Jiroskop tabanli yon takibi ----
// Ekran normali etrafindaki donus acisi (derece) jiroskopla entegre edilir.
// 0 = dik, +90 = saat yonunun tersine yatay (rot 1), -90 = saat yonunde yatay (rot 2).
#define GYRO_DEBUG            0      // 1: jiroskop / guc / performans log'lari (teshis)
#define ORIENT_GYRO_SIGN      1      // yon ters cikarsa isareti degistir (1 / -1)
#define IDLE_DIM_MS           15000  // bu kadar dokunulmazsa ekran kisilir
#define IDLE_OFF_MS           60000  // bu kadar dokunulmazsa ekran kapanir (dokunmayla uyanir)
#define IDLE_SLEEP_MS         90000  // ekran kapali + bu kadar bosta + pilde (harici guc yok) -> derin uyku
#ifndef SLEEP_TEST_ON_USB
#define SLEEP_TEST_ON_USB     0      // 1: USB takiliyken de uyu (sadece test icin)
#endif
#ifndef RSSI_OVERLAY
#define RSSI_OVERLAY          0      // 1: altta hub/yatak sinyal gucu (dBm) yazar (teshis; now_link.cpp'de NOW_RSSI=1 de gerekir)
#endif
#define BRIGHT_FULL           0xFF
#define BRIGHT_DIM            0x30
#define FLAT_ON_G             0.97f  // az bunun ustundeyse (~14 dereceden az egim) ve 1 sn boyunca oyleyse cihaz sirt ustu duz yatiyor: dondurme yapma
#define FLAT_OFF_G            0.90f  // bunun altina inince kilit acilir
#define FLAT_HOLD_TICKS       60     // ~1 sn (16 ms x 60)
#define STEP_MIN_DEG          45.0f  // bir hareketle bu kadar veya fazla donerse yon bir adim degisir
#define GYRO_BOOT_SAMPLES     40     // acilista bias olcumu (~0.7 sn, cihaz sabit olmali)
#define SETTLED_RATE_DPS      10.0f  // yumusatilmis donme hizi bunun altindaysa cihaz 'yerlesti' say (elde titreme dahil)
#define DEADBAND_DPS          2.0f   // bunun altindaki hizlar titreme sayilir, entegre edilmez
#define PWR_EXT_ON_V          4.35f  // sistem hatti (VCC) bunun ustundeyse harici 5V var: sarjda pil takiliyken ~4.5 V, pilde en fazla ~4.2 V
#define PWR_EXT_OFF_V         4.30f  // histerezis
#define STILL_TO_SWITCH       15     // ~0.25 sn yerlesik -> hareket bitti say

enum { PAGE_MASA, PAGE_YATAK, PAGE_LED, PAGE_ALL, PAGE_STATUS, PAGE_SHORTCUT, PAGE_COUNT };
#define ORDER_N 5   // siralanabilir sayfa sayisi (son sayfa = ayar sayfasi, hep en sonda)
static const int DEV_COUNT = 3; // ilk uc sayfa gercek cihaz

struct Page {
  const char *title;  // iki satir
  bool on;            // sadece cihaz sayfalari icin
  lv_obj_t *tile;
  lv_obj_t *btn;
  lv_obj_t *status;
  lv_obj_t *bar;
};

static Page s_page[PAGE_COUNT] = {
  {"MASA\nLAMBASI",   false, nullptr, nullptr, nullptr, nullptr},
  {"YATAK\nLAMBASI",  false, nullptr, nullptr, nullptr, nullptr},
  {"LED\nSERIT",      false, nullptr, nullptr, nullptr, nullptr},
  {"TUM\nISIKLAR",    false, nullptr, nullptr, nullptr, nullptr},
  {"DURUM",           false, nullptr, nullptr, nullptr, nullptr},
  {"KISAYOL\nATAMA",  false, nullptr, nullptr, nullptr, nullptr},
};
static lv_obj_t *s_tv = nullptr;
static lv_obj_t *s_dots[PAGE_COUNT];
static bool s_conf[3] = {false, false, false}; // hub'in son bildirdigi durum (baglanti koparsa buna doneriz)
static lv_obj_t *s_warn = nullptr;
static lv_obj_t *s_rssi = nullptr;             // gecici: sinyal gucu gostergesi (RSSI_OVERLAY)             // hub'a ulasilamiyor uyarisi
static int s_active = 0;                        // aktif sayfa KONUMU
static uint8_t s_order[ORDER_N] = {PAGE_MASA, PAGE_YATAK, PAGE_LED, PAGE_ALL, PAGE_STATUS}; // menu sirasi
static bool s_ext = false;           // harici guc (USB/dock) var mi
static bool s_lock = false;          // otomatik yon degistirme kilitli mi (kalici)
static lv_obj_t *s_stVal[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};   // durum sayfasi deger etiketleri
static lv_obj_t *s_tileAt[PAGE_COUNT];          // konuma gore sayfa
static bool s_rebuild = false;                  // sira degisti: arayuzu yeniden kur (olay isleyicisinin disinda)
static int s_rot = 0; // 0 dikey, 1/2 yatay

static const uint32_t COL_BG      = 0x000000;
static const uint32_t COL_BTN_ON  = 0xFFC93C;
static const uint32_t COL_BTN_OFF = 0x2A2A2A;
static const uint32_t COL_GREEN   = 0x00C853;
static const uint32_t COL_RED     = 0xE53935;
static const uint32_t COL_DOT_ON  = 0xFFFFFF;
static const uint32_t COL_DOT_OFF = 0x555555;

static int count_on() {
  int n = 0;
  for (int i = 0; i < DEV_COUNT; i++) if (s_page[i].on) n++;
  return n;
}

static void paint(Page &p, bool on, const char *text) {
  lv_obj_set_style_bg_color(p.btn, lv_color_hex(on ? COL_BTN_ON : COL_BTN_OFF), 0);
  lv_obj_set_style_bg_color(p.bar, lv_color_hex(on ? COL_GREEN : COL_RED), 0);
  lv_label_set_text(p.status, text);
  lv_obj_set_style_text_color(p.status, lv_color_hex(on ? COL_GREEN : COL_RED), 0);
}

static void refresh_all() {
  for (int i = 0; i < DEV_COUNT; i++) {
    paint(s_page[i], s_page[i].on, s_page[i].on ? "ACIK" : "KAPALI");
  }
  int n = count_on();
  char buf[16];
  snprintf(buf, sizeof(buf), "%d/%d ACIK", n, DEV_COUNT);
  paint(s_page[PAGE_ALL], n > 0, buf);
}

// ESP-NOW'dan gelen durum guncellemeleri icin (LVGL kilidi altinda cagrilmali)
void app_ui_set_state(int i, bool on) {
  if (i < 0 || i >= DEV_COUNT) return;
  s_page[i].on = on;
  refresh_all();
}

static void btn_cb(lv_event_t *e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  if (i < DEV_COUNT) {
    s_page[i].on = !s_page[i].on;
    Serial.printf("[ui] %s -> %s\n", s_page[i].title, s_page[i].on ? "ACIK" : "KAPALI");
    now_send_set(i, s_page[i].on);
  } else if (i == PAGE_ALL) {
    bool target = (count_on() == 0); // hic aciksa hepsini ac, degilse hepsini kapat
    for (int k = 0; k < DEV_COUNT; k++) { s_page[k].on = target; now_send_set(k, target); }
    Serial.printf("[ui] TUM ISIKLAR -> %s\n", target ? "ACIK" : "KAPALI");
  }
  refresh_all();
}

static void tile_changed_cb(lv_event_t *e) {
  lv_obj_t *tile = lv_tileview_get_tile_act(lv_event_get_target(e));
  for (int i = 0; i < PAGE_COUNT; i++) {
    if (s_tileAt[i] == tile) s_active = i;
  }
  now_rssi_enable(s_active < ORDER_N && s_order[s_active] == PAGE_STATUS);   // sinyal olcumu sadece durum sayfasi acikken
  for (int i = 0; i < PAGE_COUNT; i++) {
    lv_obj_set_style_bg_color(s_dots[i], lv_color_hex(i == s_active ? COL_DOT_ON : COL_DOT_OFF), 0);
  }
}

static void plain(lv_obj_t *o) {
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_shadow_width(o, 0, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t col, int w) {
  lv_obj_t *l = lv_label_create(parent);
  lv_label_set_text(l, text);
  lv_obj_set_width(l, w);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(col), 0);
  return l;
}

static void save_order() {
  Preferences prefs;
  if (prefs.begin("masa", false)) { prefs.putBytes("order", s_order, ORDER_N); prefs.end(); }
}

static void load_order() {
  uint8_t o[ORDER_N];
  Preferences prefs;
  if (!prefs.begin("masa", true)) return;
  const size_t n = prefs.getBytes("order", o, ORDER_N);
  prefs.end();
  const int cnt = (n == ORDER_N) ? ORDER_N : (n == 4 ? 4 : 0);   // eski surum 4 sayfaydi: durum sayfasi sona eklenir
  if (!cnt) return;
  bool seen[ORDER_N] = {false, false, false, false, false};
  for (int i = 0; i < cnt; i++) { if (o[i] >= cnt || seen[o[i]]) return; seen[o[i]] = true; }   // gecerli bir permutasyon degilse varsayilan
  for (int i = 0; i < cnt; i++) s_order[i] = o[i];
  if (cnt == 4) s_order[4] = PAGE_STATUS;
}

static void move_cb(lv_event_t *e) {
  const int code = (int)(intptr_t)lv_event_get_user_data(e);
  const int pg = code >> 1, later = code & 1;
  int pos = -1;
  for (int i = 0; i < ORDER_N; i++) if (s_order[i] == pg) pos = i;
  const int np = pos + (later ? 1 : -1);
  if (pos < 0 || np < 0 || np > ORDER_N - 1) return;
  const uint8_t t = s_order[pos]; s_order[pos] = s_order[np]; s_order[np] = t;
  save_order();
  Serial.printf("[ui] menu sirasi: %d %d %d %d %d\n", s_order[0], s_order[1], s_order[2], s_order[3], s_order[4]);
  s_rebuild = true;   // butonu silmeden once olay bitsin: yeniden kurma zamanlayicida
}

// Menu siralama sayfasi: her satirda cihaz adi ve iki ok. Dikeyde ok yukari/asagi, yatayda sol/sag.
static void build_order_page(lv_obj_t *tile, bool land, int W, int H) {
  static const char *kName[ORDER_N] = {"MASA", "YATAK", "LED", "HEPSI", "DURUM"};
  const char *sLater = land ? LV_SYMBOL_RIGHT : LV_SYMBOL_DOWN;
  const char *sEarlier = land ? LV_SYMBOL_LEFT : LV_SYMBOL_UP;

  lv_obj_t *title = make_label(tile, "MENU\nSIRASI", &lv_font_montserrat_28, 0xFFFFFF, land ? 170 : 250);
  if (land) lv_obj_align(title, LV_ALIGN_LEFT_MID, 12, -10);
  else      lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 26);

  const int rowH = land ? 46 : 62;
  const int y0 = land ? 16 : 100;
  const int nameX = land ? 200 : 20;
  const int btnW = land ? 60 : 56, btnH = land ? 40 : 50;
  const int btn1X = land ? W - 20 - 2 * btnW - 12 : W - 20 - 2 * btnW - 10;
  const int btn2X = btn1X + btnW + (land ? 12 : 10);

  for (int pos = 0; pos < ORDER_N; pos++) {
    const int pg = s_order[pos];
    const int y = y0 + pos * rowH;

    lv_obj_t *nm = make_label(tile, kName[pg], &lv_font_montserrat_28, 0xFFFFFF, 130);
    lv_obj_set_style_text_align(nm, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_pos(nm, nameX, y + (btnH - 30) / 2);

    for (int k = 0; k < 2; k++) {                 // k=0: one al, k=1: sona al
      const bool later = (k == 1);
      const bool ok = later ? (pos < ORDER_N - 1) : (pos > 0);
      lv_obj_t *b = lv_btn_create(tile);
      plain(b);
      lv_obj_set_size(b, btnW, btnH);
      lv_obj_set_style_radius(b, 12, 0);
      lv_obj_set_pos(b, k == 0 ? btn1X : btn2X, y);
      lv_obj_set_style_bg_color(b, lv_color_hex(ok ? 0x3A6EA5 : 0x222222), 0);
      lv_obj_t *l = lv_label_create(b);
      lv_label_set_text(l, later ? sLater : sEarlier);
      lv_obj_set_style_text_font(l, &lv_font_montserrat_28, 0);
      lv_obj_set_style_text_color(l, lv_color_hex(ok ? 0xFFFFFF : 0x555555), 0);
      lv_obj_center(l);
      if (ok) lv_obj_add_event_cb(b, move_cb, LV_EVENT_CLICKED, (void *)(intptr_t)((pg << 1) | (later ? 1 : 0)));
    }
  }
}

// ---- Durum sayfasi ----
static void set_st(int i, const char *txt, uint32_t col) {
  if (!s_stVal[i]) return;
  lv_label_set_text(s_stVal[i], txt);
  lv_obj_set_style_text_color(s_stVal[i], lv_color_hex(col), 0);
}

static uint32_t rssi_color(int8_t r) { return r > -65 ? COL_GREEN : r > -78 ? 0xFFB300 : COL_RED; }

static void status_update() {
  char b[40];
  uint32_t age;
  const uint8_t down = now_link_down_mask();
  for (int k = 0; k < 2; k++) {          // 0 = hub, 1 = yatak
    const int8_t r = now_rssi(k, &age);
    if (r && age < 30000) { snprintf(b, sizeof(b), "%d dBm", r); set_st(k, b, rssi_color(r)); }
    else set_st(k, (down & (1 << k)) ? "YOK" : "--", (down & (1 << k)) ? COL_RED : 0x8E8E93);
  }
  const float v = battery_read_vcc();
  if (s_ext) {
    snprintf(b, sizeof(b), "SARJDA %.2fV", v);
    set_st(2, b, COL_GREEN);
  } else {
    int pct = (int)((v - 3.30f) / (4.15f - 3.30f) * 100.0f + 0.5f);   // yaklasik: gercek pil olcer yok
    if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    snprintf(b, sizeof(b), "PIL ~%d%% %.2fV", pct, v);
    set_st(2, b, pct > 40 ? COL_GREEN : pct > 15 ? 0xFFB300 : COL_RED);
  }
  snprintf(b, sizeof(b), "%d", now_channel());
  set_st(3, b, 0xFFFFFF);
  snprintf(b, sizeof(b), "%s / %s", s_lock ? "KILITLI" : "SERBEST", s_rot == 0 ? "DIKEY" : "YATAY");
  set_st(4, b, s_lock ? 0xFFB300 : 0xFFFFFF);
}

static void build_status_page(lv_obj_t *tile, bool land, int W, int H) {
  static const char *kStName[5] = {"HUB", "YATAK", "GUC", "KANAL", "YON"};
  lv_obj_t *title = make_label(tile, "DURUM", &lv_font_montserrat_28, 0xFFFFFF, land ? 160 : 250);
  if (land) lv_obj_align(title, LV_ALIGN_LEFT_MID, 12, -10);
  else      lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 26);

  const int rowH = land ? 46 : 62;
  const int y0 = land ? 22 : 100;
  const int nameX = land ? 180 : 20;
  const int valW = land ? 170 : 150;
  const int valX = W - 20 - valW;
  for (int i = 0; i < 5; i++) {
    const int y = y0 + i * rowH;
    lv_obj_t *nm = make_label(tile, kStName[i], &lv_font_montserrat_14, 0x8E8E93, 70);
    lv_obj_set_style_text_align(nm, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_pos(nm, nameX, y + 9);
    lv_obj_t *v = make_label(tile, "--", &lv_font_montserrat_20, 0xFFFFFF, valW);
    lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(v, valX, y + 4);
    s_stVal[i] = v;
  }
  status_update();
}

static void build_ui(int rot) {
  const bool land = (rot != 0);
  const int W = land ? EXAMPLE_LCD_V_RES : EXAMPLE_LCD_H_RES;
  const int H = land ? EXAMPLE_LCD_H_RES : EXAMPLE_LCD_V_RES;

  lv_obj_t *scr = lv_scr_act();
  lv_obj_clean(scr);
  lv_obj_set_style_bg_color(scr, lv_color_hex(COL_BG), 0);

  s_tv = lv_tileview_create(scr);
  lv_obj_set_size(s_tv, W, H);
  lv_obj_set_style_bg_color(s_tv, lv_color_hex(COL_BG), 0);
  lv_obj_set_scrollbar_mode(s_tv, LV_SCROLLBAR_MODE_OFF);

  for (int i = 0; i < PAGE_COUNT; i++) {          // i = konum (soldan saga / yukaridan asagi)
    const int pg = (i < ORDER_N) ? s_order[i] : PAGE_SHORTCUT;
    Page &p = s_page[pg];
    lv_dir_t dir;
    if (land) {
      dir = (i == 0) ? LV_DIR_RIGHT : (i == PAGE_COUNT - 1) ? LV_DIR_LEFT : (lv_dir_t)(LV_DIR_LEFT | LV_DIR_RIGHT);
    } else {
      dir = (i == 0) ? LV_DIR_BOTTOM : (i == PAGE_COUNT - 1) ? LV_DIR_TOP : (lv_dir_t)(LV_DIR_TOP | LV_DIR_BOTTOM);
    }
    p.tile = lv_tileview_add_tile(s_tv, land ? i : 0, land ? 0 : i, dir);
    lv_obj_set_style_bg_color(p.tile, lv_color_hex(COL_BG), 0);
    s_tileAt[i] = p.tile;
    if (pg == PAGE_SHORTCUT) { build_order_page(p.tile, land, W, H); continue; }
    if (pg == PAGE_STATUS)   { build_status_page(p.tile, land, W, H); continue; }

    lv_obj_t *title = make_label(p.tile, p.title, &lv_font_montserrat_28, 0xFFFFFF, land ? 210 : 250);
    if (land) lv_obj_align(title, LV_ALIGN_CENTER, 105, -50);
    else      lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 26);

    lv_obj_t *btn = lv_btn_create(p.tile);
    plain(btn);
    lv_obj_set_size(btn, 150, 150);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    if (land) lv_obj_align(btn, LV_ALIGN_LEFT_MID, 45, -10);
    else      lv_obj_align(btn, LV_ALIGN_CENTER, 0, 20);
    lv_obj_t *icon = lv_label_create(btn);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(0x000000), 0);
    lv_obj_center(icon);

    lv_obj_t *status = make_label(p.tile, "", &lv_font_montserrat_28, COL_RED, land ? 210 : 250);
    if (land) lv_obj_align(status, LV_ALIGN_CENTER, 105, 45);
    else      lv_obj_align(status, LV_ALIGN_CENTER, 0, 125);

    lv_obj_t *bar = lv_obj_create(p.tile);
    plain(bar);
    lv_obj_set_size(bar, W, 14);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    p.btn = btn;
    p.status = status;
    p.bar = bar;

    lv_label_set_text(icon, LV_SYMBOL_POWER);
    lv_obj_add_event_cb(btn, btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)pg);
  }
  refresh_all();

  // Sayfa noktalari: dikeyde sag kenarda ust uste, yatayda altta yan yana
  const int dot = 10, gap = 14;
  const int total = PAGE_COUNT * dot + (PAGE_COUNT - 1) * gap;
  for (int i = 0; i < PAGE_COUNT; i++) {
    lv_obj_t *d = lv_obj_create(scr);
    plain(d);
    lv_obj_set_size(d, dot, dot);
    lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
    if (land) lv_obj_set_pos(d, (W - total) / 2 + i * (dot + gap), H - 36);
    else      lv_obj_set_pos(d, W - 20, (H - total) / 2 + i * (dot + gap));
    lv_obj_clear_flag(d, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(d, lv_color_hex(i == s_active ? COL_DOT_ON : COL_DOT_OFF), 0);
    s_dots[i] = d;
  }
  lv_obj_add_event_cb(s_tv, tile_changed_cb, LV_EVENT_VALUE_CHANGED, nullptr);

  // Yon degisince kullanici ayni sayfada kalsin
  lv_obj_set_tile_id(s_tv, land ? s_active : 0, land ? 0 : s_active, LV_ANIM_OFF);
}

static bool s_busy = false;
static int s_pending_rot = 0;
static lv_obj_t *s_fade = nullptr; // yon degisirken ekrani kartip geri acan siyah katman

static void fade_exec(void *o, int32_t v) {
  lv_obj_set_style_bg_opa((lv_obj_t *)o, (lv_opa_t)v, 0);
}

static void fade_start(int from, int to, int ms, lv_anim_ready_cb_t done) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, s_fade);
  lv_anim_set_exec_cb(&a, fade_exec);
  lv_anim_set_values(&a, from, to);
  lv_anim_set_time(&a, ms);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_set_ready_cb(&a, done);
  lv_anim_start(&a);
}

// ---- Ekran kisma / kapatma (AMOLED yanmasini ve isinmayi onler) ----
// ---- Ust menu (yukari kayan panel): yon kilidi ----
// Ekranin ust kenarindan asagi kaydirinca (ya da kenara dokununca) acilir, yon kilidi dugmesi icerir.
static bool s_panelOpen = false;
static lv_obj_t *s_handle = nullptr, *s_panel = nullptr, *s_scrim = nullptr;
static lv_obj_t *s_lockBtn = nullptr, *s_lockIcon = nullptr, *s_lockCap = nullptr, *s_lockInd = nullptr;
#define PANEL_H       152            // panel yuksekligi (ust koseler ekranin disinda kalir)
#define PANEL_Y_OPEN  (-22)
#define PANEL_Y_SHUT  (-(PANEL_H + 8))

static void save_lock() {
  Preferences prefs;
  if (prefs.begin("masa", false)) { prefs.putBool("lock", s_lock); prefs.end(); }
}

static void lock_visuals() {
  const uint32_t fg = s_lock ? 0x000000 : 0xFFFFFF;
  lv_obj_set_style_bg_color(s_lockBtn, lv_color_hex(s_lock ? 0xFFB300 : 0x3A3A3C), 0);
  lv_obj_set_style_text_color(s_lockIcon, lv_color_hex(fg), 0);
  lv_obj_set_style_text_color(s_lockCap, lv_color_hex(fg), 0);
  lv_label_set_text(s_lockCap, s_lock ? "YON KILITLI" : "YON SERBEST");
  if (s_lock) lv_obj_clear_flag(s_lockInd, LV_OBJ_FLAG_HIDDEN);
  else        lv_obj_add_flag(s_lockInd, LV_OBJ_FLAG_HIDDEN);
}

static void panel_y_exec(void *o, int32_t v) { lv_obj_set_y((lv_obj_t *)o, v); }

static void panel_show(bool open) {
  if (!s_panel) return;
  s_panelOpen = open;
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, s_panel);
  lv_anim_set_exec_cb(&a, panel_y_exec);
  lv_anim_set_values(&a, lv_obj_get_y(s_panel), open ? PANEL_Y_OPEN : PANEL_Y_SHUT);
  lv_anim_set_time(&a, 220);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_start(&a);
  if (open) { lv_obj_clear_flag(s_scrim, LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(s_scrim); lv_obj_move_foreground(s_panel); lv_obj_move_foreground(s_handle); }
  else      lv_obj_add_flag(s_scrim, LV_OBJ_FLAG_HIDDEN);
}

static void handle_cb(lv_event_t *e) {
  const lv_event_code_t c = lv_event_get_code(e);
  if (c == LV_EVENT_GESTURE) {
    if (lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_BOTTOM) panel_show(true);
  } else if (c == LV_EVENT_CLICKED) {
    panel_show(true);
  }
}

static void panel_gesture_cb(lv_event_t *) {
  if (lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_TOP) panel_show(false);
}

static void scrim_cb(lv_event_t *) { panel_show(false); }

static void lock_btn_cb(lv_event_t *) {
  s_lock = !s_lock;
  save_lock();
  lock_visuals();
  Serial.printf("[ui] yon kilidi: %s\n", s_lock ? "KILITLI" : "SERBEST");
}

static void panel_init() {
  Preferences prefs;
  if (prefs.begin("masa", true)) { s_lock = prefs.getBool("lock", false); prefs.end(); }

  s_scrim = lv_obj_create(lv_layer_top());
  plain(s_scrim);
  lv_obj_set_size(s_scrim, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_radius(s_scrim, 0, 0);
  lv_obj_set_style_bg_color(s_scrim, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(s_scrim, LV_OPA_50, 0);
  lv_obj_clear_flag(s_scrim, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(s_scrim, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(s_scrim, scrim_cb, LV_EVENT_CLICKED, nullptr);

  s_panel = lv_obj_create(lv_layer_top());
  plain(s_panel);
  lv_obj_set_size(s_panel, LV_PCT(100), PANEL_H);
  lv_obj_set_pos(s_panel, 0, PANEL_Y_SHUT);
  lv_obj_set_style_radius(s_panel, 24, 0);
  lv_obj_set_style_bg_color(s_panel, lv_color_hex(0x1C1C1E), 0);
  lv_obj_clear_flag(s_panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(s_panel, panel_gesture_cb, LV_EVENT_GESTURE, nullptr);

  s_lockBtn = lv_btn_create(s_panel);
  plain(s_lockBtn);
  lv_obj_set_size(s_lockBtn, 132, 92);
  lv_obj_set_style_radius(s_lockBtn, 22, 0);
  lv_obj_align(s_lockBtn, LV_ALIGN_TOP_MID, 0, 36);
  lv_obj_add_event_cb(s_lockBtn, lock_btn_cb, LV_EVENT_CLICKED, nullptr);
  s_lockIcon = lv_label_create(s_lockBtn);
  lv_label_set_text(s_lockIcon, LV_SYMBOL_LOOP);
  lv_obj_set_style_text_font(s_lockIcon, &lv_font_montserrat_28, 0);
  lv_obj_align(s_lockIcon, LV_ALIGN_TOP_MID, 0, 10);
  s_lockCap = lv_label_create(s_lockBtn);
  lv_obj_set_style_text_font(s_lockCap, &lv_font_montserrat_14, 0);
  lv_obj_align(s_lockCap, LV_ALIGN_BOTTOM_MID, 0, -10);

  lv_obj_t *grab = lv_obj_create(s_panel);      // tutamac cizgisi
  plain(grab);
  lv_obj_set_size(grab, 46, 5);
  lv_obj_set_style_radius(grab, 3, 0);
  lv_obj_set_style_bg_color(grab, lv_color_hex(0x666666), 0);
  lv_obj_clear_flag(grab, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_align(grab, LV_ALIGN_BOTTOM_MID, 0, -8);

  s_handle = lv_obj_create(lv_layer_top());     // ust kenar: gorunmez, asagi kaydirma / dokunma ile paneli acar
  plain(s_handle);
  lv_obj_set_size(s_handle, LV_PCT(100), 26);
  lv_obj_set_pos(s_handle, 0, 0);
  lv_obj_set_style_radius(s_handle, 0, 0);
  lv_obj_set_style_bg_opa(s_handle, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(s_handle, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(s_handle, handle_cb, LV_EVENT_ALL, nullptr);

  s_lockInd = lv_label_create(lv_layer_top());  // kilitliyken ust ortada kucuk gosterge
  lv_label_set_text(s_lockInd, LV_SYMBOL_LOOP);
  lv_obj_set_style_text_font(s_lockInd, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_lockInd, lv_color_hex(0xFFB300), 0);
  lv_obj_align(s_lockInd, LV_ALIGN_TOP_MID, 0, 8);

  lock_visuals();
}

static bool s_wokeFromSleep = false; // derin uykudan dokunmayla uyandik
// Derin uykuda korunan veriler (RTC bellek): son bilinen lamba durumlari ve jiroskop bias'i
RTC_DATA_ATTR static uint8_t rtc_state[3] = {0, 0, 0};
RTC_DATA_ATTR static bool rtc_state_valid = false;
RTC_DATA_ATTR static float rtc_bz = 0;
RTC_DATA_ATTR static bool rtc_bz_valid = false;

static int s_scr = 0; // 0 tam parlak, 1 kisik, 2 kapali

static void idle_tick(lv_timer_t *) {
  if (g_lcd_wake_req) {            // kapali ekrani uyandiran dokunus
    g_lcd_wake_req = false;
    lv_disp_trig_activity(nullptr);
  }
  const uint32_t idle = lv_disp_get_inactive_time(nullptr);
  if (s_panelOpen && idle >= 5000) panel_show(false);   // panel 5 sn dokunulmazsa kapanir
  const int want = (idle >= IDLE_OFF_MS) ? 2 : (idle >= IDLE_DIM_MS) ? 1 : 0;
  if (want == 2 && idle >= IDLE_SLEEP_MS && (SLEEP_TEST_ON_USB || !s_ext)) {
    Serial.printf("[sleep] derin uykuya giriliyor (bosta %lu ms)\n", (unsigned long)idle);
    Serial.flush();
    sleep_deep_poll();               // donmez; dokunmayla yeniden acilir
  }
  if (want == s_scr) return;
  const int prev = s_scr;
  s_scr = want;
  if (want == 0 && prev != 0) now_query(); // ekran acilinca durumu tazele
  set_amoled_backlight(want == 2 ? 0x00 : want == 1 ? BRIGHT_DIM : BRIGHT_FULL);
  g_lcd_screen_off = (want == 2);
  Serial.printf("[scr] ekran: %s (bosta %lu ms)\n", want == 2 ? "KAPALI" : want == 1 ? "KISIK" : "ACIK", (unsigned long)idle);
}

// ---- ESP-NOW: hub'dan gelen durumu arayuze isle ----
static void now_tick(lv_timer_t *) {
  static int stTick = 0;
  if (s_active < ORDER_N && s_order[s_active] == PAGE_STATUS) {   // durum sayfasi acik: 0.5 sn'de bir yenile, 2 sn'de bir taze sinyal orneği iste
    if (++stTick % 5 == 0) status_update();
    if (stTick % 20 == 0) now_query();
  } else stTick = 0;
  uint8_t st[4];
  if (now_take_state(st)) {
    for (int k = 0; k < DEV_COUNT; k++) {
      if (st[k] == 0xFF) continue;   // bu cihaz henuz bildirilmedi (kapali/erisilemiyor): son bilinen durumu koru
      s_conf[k] = st[k]; s_page[k].on = st[k]; rtc_state[k] = st[k];
    }
    rtc_state_valid = true;
    refresh_all();
  }
  if (now_take_failed()) {           // komut hub'a ulasamadi: ekrani gercek duruma dondur
    for (int k = 0; k < DEV_COUNT; k++) s_page[k].on = s_conf[k];
    refresh_all();
  }
#if RSSI_OVERLAY
  {
    char b[48], h[12], y[12];
    uint32_t ah, ay;
    int8_t rh = now_rssi(0, &ah), ry = now_rssi(1, &ay);
    if (rh && ah < 30000) snprintf(h, sizeof(h), "%d", rh); else snprintf(h, sizeof(h), "--");
    if (ry && ay < 30000) snprintf(y, sizeof(y), "%d", ry); else snprintf(y, sizeof(y), "--");
    snprintf(b, sizeof(b), "hub %s  yatak %s dBm", h, y);
    lv_label_set_text(s_rssi, b);
  }
#endif
  const uint8_t down = now_link_down_mask();      // bit0 hub, bit1 yatak
  if (!down) {
    lv_obj_add_flag(s_warn, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_label_set_text(s_warn, down == 3 ? LV_SYMBOL_WARNING " HUB+YATAK" : (down & 1) ? LV_SYMBOL_WARNING " HUB" : LV_SYMBOL_WARNING " YATAK");
    lv_obj_clear_flag(s_warn, LV_OBJ_FLAG_HIDDEN);
  }
}

// ---- Sarj ikonu (sag ust kose) ----
static lv_obj_t *s_chg = nullptr;

static void chg_place() {
  if (s_chg) lv_obj_align(s_chg, LV_ALIGN_TOP_RIGHT, -14, 10);
}

static void power_tick(lv_timer_t *) {
  const float v = battery_read_vcc();
  const bool ext = s_ext ? (v > PWR_EXT_OFF_V) : (v > PWR_EXT_ON_V);
  if (ext == s_ext) return;
  s_ext = ext;
  if (ext) lv_obj_clear_flag(s_chg, LV_OBJ_FLAG_HIDDEN);
  else     lv_obj_add_flag(s_chg, LV_OBJ_FLAG_HIDDEN);
  Serial.printf("[pwr] harici guc %s (vcc=%.2f V)\n", ext ? "VAR" : "YOK", v);
}

static void save_rot(int rot) {
  Preferences prefs;
  if (prefs.begin("masa", false)) { prefs.putInt("rot", rot); prefs.end(); }
}

static void fade_in_done(lv_anim_t *) { s_busy = false; }

static void fade_out_done(lv_anim_t *) {
  // Ekran tamamen siyah: rahatca dondur ve yeni arayuzu kur
  s_rot = s_pending_rot;
  lcd_set_rotation(s_rot);
  build_ui(s_rot);
  save_rot(s_rot);
  chg_place();
  lv_obj_set_size(s_fade, LV_PCT(100), LV_PCT(100));
  lv_obj_move_foreground(s_fade);
  fade_start(255, 0, 280, fade_in_done);
}

static float s_acc = 0.0f;   // son yerlesmeden beri biriken donus (derece)
static int s_canon = 0;      // mantiksal yon: 0 dik, 90 (rot 1), -90 (rot 2), 180 ters
static float s_bz = 0;      // gz bias (durgunken olculen)
static float s_mz = 0;      // gz'nin hizli takip eden ortalamasi
static float s_dev = 0;     // ortalamadan sapma (titreme olcusu)
static float s_rate = 0;    // yumusatilmis |donme hizi|
static float s_sz = 0;      // acilis bias toplami
static uint32_t s_last_us = 0;
static int s_boot_n = 0, s_flat = 0, s_still = 0;
static float s_az = 0;         // az'nin yumusatilmis degeri
static bool s_lying = false;   // sirt ustu duz yatiyor mu
static int s_flatCnt = 0;      // az > FLAT_ON_G art arda kac tick
static uint32_t s_dbg_ms = 0, s_perf_ms = 0;

static void layout_tick(lv_timer_t *) {
  if (s_rebuild && !s_busy) {
    s_rebuild = false;
    build_ui(s_rot);   // sira degisti: yeni sirayla kur, ayar sayfasinda kal
    refresh_all();
  }
}

static void gyro_tick(lv_timer_t *) {
  imu_update();
  const uint32_t now_us = micros();
  float dt = (now_us - s_last_us) * 1e-6f;
  s_last_us = now_us;
  if (dt <= 0.0f || dt > 0.2f) dt = 0.016f;

  const float gz = imu_get_gz(); // ekran normali etrafindaki donus
  const float az = imu_get_az();

  if (s_boot_n < GYRO_BOOT_SAMPLES) {
    if (s_wokeFromSleep && rtc_bz_valid) { // uykudan dokunmayla uyandik: cihaz elde olabilir, kayitli bias'i kullan
      s_bz = rtc_bz;
      s_boot_n = GYRO_BOOT_SAMPLES;
      s_mz = gz;
      s_az = az;
      return;
    }
    // acilista cihazin sabit oldugunu varsay, bias'i olc
    s_sz += gz;
    if (++s_boot_n == GYRO_BOOT_SAMPLES) s_bz = s_sz / s_boot_n;
    s_mz = gz;
    s_az = az;
    return;
  }

  // Hizli takip eden ortalama: elde titreme ortalamada erir
  s_mz += 0.05f * (gz - s_mz);
  s_dev += 0.1f * (fabsf(gz - s_mz) - s_dev);

  // Cihaz uzun sure cok sabitse (bias yanlis olculmusse) bias'i yeniden al
  if (s_dev < 1.0f) s_flat++; else s_flat = 0;
  if (s_flat >= 100) s_bz = s_mz;
  else if (s_flat >= 20) s_bz += 0.02f * (s_mz - s_bz);
  if (s_flat >= 20) { rtc_bz = s_bz; rtc_bz_valid = true; }

  float rate = gz - s_bz;
  s_rate += 0.1f * (fabsf(rate) - s_rate);
  const bool settled = s_rate < SETTLED_RATE_DPS;

  if (fabsf(rate) < DEADBAND_DPS) rate = 0.0f; // titreme kaymaya yol acmasin
  s_acc += ORIENT_GYRO_SIGN * rate * dt;

  // Sirt ustu duz yatiyor mu? (ekran yukari bakarken az ~ +1)
  s_az += 0.05f * (az - s_az);
  if (s_az > FLAT_ON_G) { if (s_flatCnt < 1000) s_flatCnt++; } else s_flatCnt = 0;
  if (!s_lying && s_flatCnt >= FLAT_HOLD_TICKS) { s_lying = true;  Serial.printf("[ui] sirt ustu: yon kilitli (az=%.2f)\n", s_az); }
  else if (s_lying && s_az < FLAT_OFF_G) { s_lying = false; Serial.printf("[ui] kaldirildi: yon serbest (az=%.2f)\n", s_az); }

  const uint32_t ms = millis();
#if GYRO_DEBUG
  if (ms - s_dbg_ms > 500) {
    s_dbg_ms = ms;
    Serial.printf("[gyro] acc=%+.0f canon=%d gz=%+.1f settled=%d az=%.2f lying=%d rot=%d\n", s_acc, s_canon, gz, settled, s_az, s_lying, s_rot);
  }
#endif
#if GYRO_DEBUG
  if (ms - s_perf_ms > 3000) {
    s_perf_ms = ms;
    Serial.printf("[pwr] vcc=%.2f V\n", battery_read_vcc());
    Serial.printf("[perf] handler_max=%lu us  touch_gap_max=%lu us  touch_miss=%lu\n",
                  (unsigned long)g_perf_handler_max_us, (unsigned long)g_perf_touch_gap_max_us,
                  (unsigned long)g_perf_touch_miss);
    g_perf_handler_max_us = 0;
    g_perf_touch_gap_max_us = 0;
  }

#endif
  if (s_lying) { s_acc = 0.0f; s_still = 0; return; } // duz yatarken yon degistirme
  if (s_lock)  { s_acc = 0.0f; s_still = 0; return; } // kullanici yonu kilitledi
  if (!settled) { s_still = 0; return; }
  if (++s_still < STILL_TO_SWITCH) return;
  if (s_busy || g_lcd_touched) return;

  // Hareket bitti. Yeterince dondurduysa yon degisir; degilse kucuk kayma yavasca sonumlenir.
  if (fabsf(s_acc) < STEP_MIN_DEG) { s_acc *= 0.995f; return; }

  int steps = (int)lroundf(s_acc / 90.0f);
  if (steps == 0) steps = (s_acc > 0) ? 1 : -1;
  s_acc = 0.0f;
  int c = s_canon + 90 * steps;
  while (c > 180) c -= 360;
  while (c <= -180) c += 360;
  s_canon = c;

  int cand = s_rot; // 180 (ters dik) desteklenmiyor, mevcut yonde kal
  if (c == 0) cand = 0;
  else if (c == 90) cand = 1;
  else if (c == -90) cand = 2;
  if (cand == s_rot) return;

  Serial.printf("[ui] yon: %d -> %d (%d adim)\n", s_rot, cand, steps);
  s_pending_rot = cand;
  s_busy = true;
  fade_start(0, 255, 180, fade_out_done); // once kararsin, sonra donsun
}

void app_ui_init(void) {
  load_order();
  s_wokeFromSleep = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER);
  if (rtc_state_valid) {             // son bilinen durumla ac, hub'dan gelen taze durum hemen guncellesin
    for (int k = 0; k < DEV_COUNT; k++) { s_page[k].on = rtc_state[k]; s_conf[k] = rtc_state[k]; }
  }
  // Son yon hatirlanir: dock'ta / masada birakilan yonde acilir
  int saved = 0;
  Preferences prefs;
  if (prefs.begin("masa", true)) { saved = prefs.getInt("rot", 0); prefs.end(); }
  if (saved == 1 || saved == 2) {
    s_rot = saved;
    s_canon = (saved == 1) ? 90 : -90;
    lcd_set_rotation(s_rot);
  }
  build_ui(s_rot);

  s_fade = lv_obj_create(lv_layer_top());
  plain(s_fade);
  lv_obj_set_size(s_fade, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_radius(s_fade, 0, 0);
  lv_obj_set_style_bg_color(s_fade, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(s_fade, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(s_fade, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));

  panel_init();

  s_chg = lv_label_create(lv_layer_top());
  lv_label_set_text(s_chg, LV_SYMBOL_CHARGE);
  lv_obj_set_style_text_font(s_chg, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(s_chg, lv_color_hex(0xFFFFFF), 0);
  lv_obj_add_flag(s_chg, LV_OBJ_FLAG_HIDDEN);
  chg_place();
  power_tick(nullptr);
  lv_timer_create(idle_tick, 200, nullptr);

  s_warn = lv_label_create(lv_layer_top());
  lv_label_set_text(s_warn, LV_SYMBOL_WARNING);
  lv_obj_set_style_text_font(s_warn, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_warn, lv_color_hex(0xFFB300), 0);
  lv_obj_align(s_warn, LV_ALIGN_TOP_LEFT, 14, 10);
  s_rssi = lv_label_create(lv_layer_top());
  lv_obj_set_style_text_font(s_rssi, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(s_rssi, lv_color_hex(0x888888), 0);
  lv_label_set_text(s_rssi, "");
  lv_obj_align(s_rssi, LV_ALIGN_BOTTOM_MID, 0, -22);
  lv_timer_create(now_tick, 100, nullptr);
  lv_timer_create(power_tick, 500, nullptr);

  s_last_us = micros();
  lv_timer_create(layout_tick, 40, nullptr);
  lv_timer_create(gyro_tick, 16, nullptr);
}
