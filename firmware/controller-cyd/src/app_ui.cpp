#include <Arduino.h>
#include <lvgl.h>
#include "fonts.h"
#include "hal_cyd.h"
#include "panel_cfg.h"
#include "now_link.h"
#include "i18n.h"
#include "app_ui.h"

// Kapi paneli arayuzu (320x240, yatay).
//  - Ana ekran: 4'lu / 3'lu / 2'li / 1'li duzen; karolar tek ortak siraya gore dizilir. 3'lu, 2'li, 1'li duzende
//    sayfalar yana kaydirilir (alttaki noktalara dokunmak da sayfa degistirir).
//  - Ayar menusu (sag ustteki disli): DUZEN (duzen secimi + siralama + gizleme), EKRAN (parlaklik, otomatik parlaklik,
//    ekran kapanma), DURUM (sinyal gucu, kanal, dokunmatik kalibrasyonu).
//  - Komutlar mutlak degerdir; karsi cihazin ayni seq ile dondurdugu STATE ile onaylanir (now_link.cpp).

static const uint32_t C_BG = 0x000000, C_ON = 0xFFC93C, C_OFF = 0x2A2A2A, C_GREEN = 0x00C853, C_RED = 0xE53935;
static const uint32_t C_AMBER = 0xFFB300, C_BLUE = 0x3A6EA5, C_GREY = 0x8E8E93, C_CARD = 0x1C1C1E, C_WHITE = 0xFFFFFF;
static const char *NAME_FULL[IT_COUNT] = T_ITEM_FULL;
static const char *NAME_SHORT[IT_COUNT] = T_ITEM_SHORT;

// ---------------------------------------------------------------- yardimcilar
static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t bg, int radius = 0, bool opaque = true) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_color(o, lv_color_hex(bg), 0);
  lv_obj_set_style_bg_opa(o, opaque ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(o, radius, 0);
  lv_obj_clear_flag(o, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));   // varsayilan: tiklanamaz
  return o;
}

static lv_obj_t *text(lv_obj_t *parent, const char *s, const lv_font_t *f, uint32_t col, int x, int y, int w = 0,
                      lv_text_align_t al = LV_TEXT_ALIGN_LEFT) {
  lv_obj_t *l = lv_label_create(parent);
  lv_label_set_text(l, s);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(col), 0);
  lv_obj_set_style_text_align(l, al, 0);
  lv_obj_set_pos(l, x, y);
  if (w) { lv_obj_set_width(l, w); lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP); }
  return l;
}

static void clickable(lv_obj_t *o, lv_event_cb_t cb, intptr_t user, lv_event_code_t code = LV_EVENT_CLICKED) {
  lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(o, cb, code, (void *)user);
}

// ---------------------------------------------------------------- durum
static bool s_on[3] = {false, false, false};     // ekranda gosterilen durum
static bool s_conf[3] = {false, false, false};   // cihazin son bildirdigi durum (komut ulasmazsa buna doneriz)
static lv_obj_t *s_main = nullptr, *s_set = nullptr;

static bool hidden(int it) { return g_set.hiddenMask & (1 << it); }

// gorunen karolar (siraya gore, gizliler cikarilmis)
static int s_vis[IT_COUNT];
static int s_nvis = 0;
static void compute_vis() {
  s_nvis = 0;
  for (int i = 0; i < IT_COUNT; i++) if (!hidden(g_set.order[i])) s_vis[s_nvis++] = g_set.order[i];
}

// "Tum isiklar" sadece gorunen cihazlari sayar ve onlari yonetir.
static void count_devices(int &cnt, int &tot) {
  cnt = 0; tot = 0;
  for (int d = 0; d < 3; d++) if (!hidden(d)) { tot++; if (s_on[d]) cnt++; }
}

static void item_state(int it, bool &on, char *stat, size_t n) {
  if (it < IT_ALL) {
    on = s_on[it];
    snprintf(stat, n, "%s", on ? T_ON : T_OFF);
  } else {
    int cnt, tot;
    count_devices(cnt, tot);
    on = cnt > 0;
    if (on) snprintf(stat, n, T_N_ON, cnt, tot);
    else snprintf(stat, n, "%s", T_OFF);
  }
}

// ---------------------------------------------------------------- ana ekran: karolar
struct TileUi { lv_obj_t *tile, *circle, *icon, *name, *stat, *bar; };
static TileUi s_t[IT_COUNT];

static void refresh_tile(int it) {
  TileUi &t = s_t[it];
  if (!t.tile) return;
  bool on;
  char stat[24];
  item_state(it, on, stat, sizeof(stat));
  lv_obj_set_style_bg_color(t.tile, lv_color_hex(on ? C_ON : C_OFF), 0);
  lv_obj_set_style_bg_color(t.circle, lv_color_hex(on ? 0xD6AA30 : 0x3A3A3C), 0);
  lv_obj_set_style_text_color(t.icon, lv_color_hex(on ? 0x000000 : 0xAAAAAA), 0);
  lv_obj_set_style_text_color(t.name, lv_color_hex(on ? 0x000000 : C_WHITE), 0);
  lv_label_set_text(t.stat, stat);
  lv_obj_set_style_text_color(t.stat, lv_color_hex(on ? 0x000000 : C_RED), 0);
  lv_obj_set_style_bg_color(t.bar, lv_color_hex(on ? C_GREEN : C_RED), 0);
}

static void refresh_tiles() { for (int i = 0; i < IT_COUNT; i++) refresh_tile(i); }

static void tile_cb(lv_event_t *e) {
  const int it = (int)(intptr_t)lv_event_get_user_data(e);
  if (it < IT_ALL) {
    s_on[it] = !s_on[it];
    now_send_set(it, s_on[it]);
    Serial.printf("[ui] %s -> %s\n", NAME_SHORT[it], s_on[it] ? "ACIK" : "KAPALI");
  } else {
    int cnt, tot;
    count_devices(cnt, tot);
    if (!tot) return;
    const bool target = (cnt == 0);   // hic aciksa hepsini ac, degilse hepsini kapat
    for (int d = 0; d < 3; d++) if (!hidden(d)) { s_on[d] = target; now_send_set(d, target); }
    Serial.printf("[ui] TUM ISIKLAR -> %s\n", target ? "ACIK" : "KAPALI");
  }
  refresh_tiles();
}

// j: sayfadaki sira; duzene gore konum ve gorunum modu (s = kucuk yatay, c = dar sutun, b = buyuk)
static void geom(int j, int &x, int &y, int &w, int &h, char &mode) {
  switch (g_set.layout) {
    case 4: x = 8 + (j % 2) * 156; y = 6 + (j / 2) * 102; w = 148; h = 94; mode = 's'; break;
    case 3: x = 8 + j * 104; y = 6; w = 96; h = 180; mode = 'c'; break;
    case 2: x = 8 + j * 156; y = 6; w = 148; h = 180; mode = 'b'; break;
    default: x = 34; y = 6; w = 252; h = 180; mode = 'b'; break;
  }
}

static void make_tile(lv_obj_t *parent, int it, int j) {
  int x, y, w, h;
  char mode;
  geom(j, x, y, w, h, mode);
  TileUi &t = s_t[it];
  t.tile = box(parent, x, y, w, h, C_OFF, 14);
  lv_obj_set_style_clip_corner(t.tile, true, 0);
  clickable(t.tile, tile_cb, it);

  const lv_font_t *iconFont;
  int d;
  if (mode == 's') {
    d = 40; iconFont = &lv_font_montserrat_20;
    t.circle = box(t.tile, 10, (h - d) / 2 - 3, d, d, 0x3A3A3C, LV_RADIUS_CIRCLE);
    t.name = text(t.tile, NAME_SHORT[it], &lv_font_tr_14, C_WHITE, 58, h / 2 - 22, w - 62);
    t.stat = text(t.tile, "", &lv_font_tr_14, C_RED, 58, h / 2 - 2, w - 62);
  } else if (mode == 'c') {
    d = 58; iconFont = &lv_font_montserrat_28;
    t.name = text(t.tile, NAME_SHORT[it], &lv_font_tr_14, C_WHITE, 2, 12, w - 4, LV_TEXT_ALIGN_CENTER);
    t.circle = box(t.tile, (w - d) / 2, (h - d) / 2 + 2, d, d, 0x3A3A3C, LV_RADIUS_CIRCLE);
    t.stat = text(t.tile, "", &lv_font_tr_14, C_RED, 0, h - 16 - 17, w, LV_TEXT_ALIGN_CENTER);
  } else {
    d = (w > 200) ? 96 : 70;
    iconFont = (d > 80) ? &lv_font_montserrat_48 : &lv_font_montserrat_28;
    t.name = text(t.tile, NAME_FULL[it], &lv_font_tr_16, C_WHITE, 0, 12, w, LV_TEXT_ALIGN_CENTER);
    t.circle = box(t.tile, (w - d) / 2, (h - d) / 2 + 4, d, d, 0x3A3A3C, LV_RADIUS_CIRCLE);
    t.stat = text(t.tile, "", &lv_font_tr_20, C_RED, 0, h - 16 - 24, w, LV_TEXT_ALIGN_CENTER);
  }
  t.icon = lv_label_create(t.circle);
  lv_label_set_text(t.icon, LV_SYMBOL_POWER);
  lv_obj_set_style_text_font(t.icon, iconFont, 0);
  lv_obj_center(t.icon);
  t.bar = box(t.tile, 0, h - 5, w, 5, C_RED);
}

// ---------------------------------------------------------------- ana ekran: ust cubuk, noktalar, oklar
static lv_obj_t *s_tv = nullptr, *s_pageTile[IT_COUNT];
static lv_obj_t *s_dotObj[IT_COUNT], *s_arrL = nullptr, *s_arrR = nullptr;
static lv_obj_t *s_linkDot = nullptr, *s_linkWarn = nullptr, *s_linkText = nullptr;
static int s_pages = 1, s_page = 0;
static uint8_t s_lastDown = 0xFF;

static void update_page_ui() {
  for (int p = 0; p < s_pages; p++) lv_obj_set_style_bg_color(s_dotObj[p], lv_color_hex(p == s_page ? C_WHITE : 0x555555), 0);
  if (s_arrL) lv_obj_set_style_text_color(s_arrL, lv_color_hex(s_page > 0 ? C_WHITE : 0x555555), 0);
  if (s_arrR) lv_obj_set_style_text_color(s_arrR, lv_color_hex(s_page < s_pages - 1 ? C_WHITE : 0x555555), 0);
}

static void goto_page(int p) {
  if (p < 0 || p >= s_pages || !s_tv) return;
  lv_obj_set_tile_id(s_tv, p, 0, LV_ANIM_ON);
}

static void page_changed_cb(lv_event_t *) {
  lv_obj_t *act = lv_tileview_get_tile_act(s_tv);
  for (int p = 0; p < s_pages; p++) if (s_pageTile[p] == act) s_page = p;
  update_page_ui();
}

static void dot_cb(lv_event_t *e) { goto_page((int)(intptr_t)lv_event_get_user_data(e)); }
static void arrow_cb(lv_event_t *e) { goto_page(s_page + (int)(intptr_t)lv_event_get_user_data(e)); }

static void update_link_bar() {
  if (!s_linkText) return;
  const uint8_t down = now_link_down_mask();
  if (down == s_lastDown) return;
  s_lastDown = down;
  if (!down) {
    lv_obj_clear_flag(s_linkDot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_linkWarn, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_linkText, T_CONNECTED);
    lv_obj_set_style_text_color(s_linkText, lv_color_hex(C_GREY), 0);
    lv_obj_set_x(s_linkText, 24);
  } else {
    lv_obj_add_flag(s_linkDot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_linkWarn, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_linkText, down == 3 ? T_WARN_BOTH : (down & 1) ? T_WARN_HUB : T_WARN_BED);
    lv_obj_set_style_text_color(s_linkText, lv_color_hex(C_AMBER), 0);
    lv_obj_set_x(s_linkText, 28);
  }
}

static void show_settings();

static void gear_cb(lv_event_t *) { show_settings(); }

static void build_main() {
  memset(s_t, 0, sizeof(s_t));
  s_main = lv_obj_create(nullptr);
  lv_obj_remove_style_all(s_main);
  lv_obj_set_size(s_main, SCREEN_W, SCREEN_H);
  lv_obj_set_style_bg_color(s_main, lv_color_hex(C_BG), 0);
  lv_obj_set_style_bg_opa(s_main, LV_OPA_COVER, 0);
  lv_obj_clear_flag(s_main, LV_OBJ_FLAG_SCROLLABLE);

  compute_vis();
  const int per = g_set.layout;
  s_pages = (s_nvis + per - 1) / per;
  if (s_pages < 1) s_pages = 1;
  s_page = 0;

  s_tv = lv_tileview_create(s_main);
  lv_obj_remove_style_all(s_tv);
  lv_obj_set_pos(s_tv, 0, 30);
  lv_obj_set_size(s_tv, SCREEN_W, SCREEN_H - 30);
  lv_obj_set_style_bg_opa(s_tv, LV_OPA_TRANSP, 0);
  lv_obj_set_scrollbar_mode(s_tv, LV_SCROLLBAR_MODE_OFF);
  for (int p = 0; p < s_pages; p++) {
    lv_dir_t dir = (s_pages == 1) ? LV_DIR_NONE : (p == 0) ? LV_DIR_RIGHT : (p == s_pages - 1) ? LV_DIR_LEFT : LV_DIR_HOR;
    s_pageTile[p] = lv_tileview_add_tile(s_tv, p, 0, dir);
    // remove_style_all burada KULLANMA: tileview'in verdigi boyut/konum yerel stil oldugu icin onunla silinir
    lv_obj_set_style_bg_opa(s_pageTile[p], LV_OPA_TRANSP, 0);
  }
  for (int k = 0; k < s_nvis; k++) make_tile(s_pageTile[k / per], s_vis[k], k % per);
  lv_obj_add_event_cb(s_tv, page_changed_cb, LV_EVENT_VALUE_CHANGED, nullptr);

  // ust cubuk: baglanti durumu + disli
  s_linkDot = box(s_main, 10, 11, 7, 7, C_GREEN, LV_RADIUS_CIRCLE);
  s_linkWarn = text(s_main, LV_SYMBOL_WARNING, &lv_font_montserrat_14, C_AMBER, 10, 7);
  s_linkText = text(s_main, "", &lv_font_tr_12, C_GREY, 24, 7, 230);
  s_lastDown = 0xFF;
  update_link_bar();
  lv_obj_t *gear = box(s_main, SCREEN_W - 52, 0, 52, 40, 0, 0, false);
  clickable(gear, gear_cb, 0);
  lv_obj_t *gl = lv_label_create(gear);
  lv_label_set_text(gl, LV_SYMBOL_SETTINGS);
  lv_obj_set_style_text_font(gl, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(gl, lv_color_hex(0xBBBBBB), 0);
  lv_obj_align(gl, LV_ALIGN_CENTER, 0, -2);

  // sayfa noktalari (dokununca sayfa degisir)
  memset(s_dotObj, 0, sizeof(s_dotObj));
  if (s_pages > 1) {
    const int pitch = 18, w = s_pages * pitch;
    for (int p = 0; p < s_pages; p++) {
      lv_obj_t *hit = box(s_main, (SCREEN_W - w) / 2 + p * pitch, 212, pitch, 28, 0, 0, false);
      clickable(hit, dot_cb, p);
      s_dotObj[p] = box(hit, (pitch - 8) / 2, 14, 8, 8, 0x555555, LV_RADIUS_CIRCLE);
    }
  }
  // 1'li duzende kenarlarda ok alanlari
  s_arrL = s_arrR = nullptr;
  if (g_set.layout == 1 && s_pages > 1) {
    for (int k = 0; k < 2; k++) {
      lv_obj_t *z = box(s_main, k ? SCREEN_W - 30 : 2, 90, 28, 72, 0, 0, false);
      clickable(z, arrow_cb, k ? 1 : -1);
      lv_obj_t *l = lv_label_create(z);
      lv_label_set_text(l, k ? LV_SYMBOL_RIGHT : LV_SYMBOL_LEFT);
      lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
      lv_obj_center(l);
      (k ? s_arrR : s_arrL) = l;
    }
  }
  if (s_pages > 1) update_page_ui();
  refresh_tiles();
}

// ---------------------------------------------------------------- ekran kapanma / parlaklik
extern bool g_ui_freeze;   // gelistirme: ekran goruntusu alirken durum degisimi/ekran kapanmasi durur
static bool s_off = false;
static float s_autoPct = 70;
static int s_lastBl = -1;

static void apply_backlight() {
  int p = s_off ? 0 : (g_set.autoBright ? (int)s_autoPct : g_set.brightness);
  if (p == s_lastBl) return;
  s_lastBl = p;
  hal_set_backlight(p);
}

static void wake_cb() {
  if (!s_off) return;
  s_off = false;
  hal_set_screen_off(false);
  apply_backlight();
  lv_disp_trig_activity(nullptr);
  now_query();
}

static void idle_tick(lv_timer_t *) {
  if (g_ui_freeze) return;
  if (g_set.autoBright) s_autoPct += (hal_light_percent() - s_autoPct) * 0.25f;
  const uint16_t secs = OFF_SECS[g_set.offIdx];
  const bool want = secs > 0 && lv_disp_get_inactive_time(nullptr) >= (uint32_t)secs * 1000UL;
  if (want != s_off) {
    s_off = want;
    hal_set_screen_off(s_off);
    if (!s_off) now_query();
    Serial.printf("[scr] ekran %s\n", s_off ? "kapali" : "acik");
  }
  apply_backlight();
}

// ---------------------------------------------------------------- ESP-NOW durum akisi
bool g_ui_freeze = false;

static void now_tick(lv_timer_t *) {
  if (g_ui_freeze) return;
  uint8_t st[4];
  if (now_take_state(st)) {
    for (int k = 0; k < 3; k++) {
      if (st[k] == 0xFF) continue;   // bu cihaz henuz bildirilmedi: son bilinen durumu koru
      s_conf[k] = st[k];
      s_on[k] = st[k];
    }
    refresh_tiles();
  }
  if (now_take_failed()) {           // komut ulasmadi: ekrani gercek duruma dondur
    for (int k = 0; k < 3; k++) s_on[k] = s_conf[k];
    refresh_tiles();
  }
  update_link_bar();
}

// ================================================================ AYAR MENUSU
static lv_obj_t *s_tabBtn[3], *s_tabLbl[3], *s_tabBox[3];
static lv_obj_t *s_card[4], *s_cardIcon[4][4], *s_cardLbl[4], *s_rows = nullptr;
static lv_obj_t *s_slider = nullptr, *s_switch = nullptr, *s_btnm = nullptr, *s_stVal[3];
static lv_timer_t *s_stTimer = nullptr;
static int s_tab = 0;

static void leave_settings();

static void back_cb(lv_event_t *) { leave_settings(); }

// ---- DUZEN sekmesi ----
static const int CARD_LAYOUT[4] = {4, 3, 2, 1};

static void style_cards() {
  for (int k = 0; k < 4; k++) {
    const bool sel = g_set.layout == CARD_LAYOUT[k];
    lv_obj_set_style_bg_color(s_card[k], lv_color_hex(sel ? 0x26303F : C_CARD), 0);
    lv_obj_set_style_border_width(s_card[k], sel ? 2 : 0, 0);
    lv_obj_set_style_border_color(s_card[k], lv_color_hex(C_BLUE), 0);
    lv_obj_set_style_text_color(s_cardLbl[k], lv_color_hex(sel ? C_WHITE : C_GREY), 0);
    for (int r = 0; r < 4; r++) if (s_cardIcon[k][r]) lv_obj_set_style_bg_color(s_cardIcon[k][r], lv_color_hex(sel ? C_ON : 0x666666), 0);
  }
}

static void card_cb(lv_event_t *e) {
  g_set.layout = CARD_LAYOUT[(int)(intptr_t)lv_event_get_user_data(e)];
  settings_save();
  style_cards();
}

static void rows_rebuild();
static void rows_async(void *) { rows_rebuild(); }

static void move_cb(lv_event_t *e) {
  const int code = (int)(intptr_t)lv_event_get_user_data(e);   // pos*2 + (1 = asagi)
  const int pos = code >> 1, np = pos + ((code & 1) ? 1 : -1);
  if (np < 0 || np >= IT_COUNT) return;
  const uint8_t t = g_set.order[pos];
  g_set.order[pos] = g_set.order[np];
  g_set.order[np] = t;
  settings_save();
  lv_async_call(rows_async, nullptr);
}

static void eye_cb(lv_event_t *e) {
  const int it = (int)(intptr_t)lv_event_get_user_data(e);
  if (!hidden(it)) {
    int visible = 0;
    for (int i = 0; i < IT_COUNT; i++) if (!hidden(i)) visible++;
    if (visible <= 1) return;                 // en az bir karo gorunur kalir
    g_set.hiddenMask |= (1 << it);
  } else {
    g_set.hiddenMask &= ~(1 << it);
  }
  settings_save();
  lv_async_call(rows_async, nullptr);
}

static void rows_rebuild() {
  if (!s_rows) return;
  lv_obj_clean(s_rows);
  for (int pos = 0; pos < IT_COUNT; pos++) {
    const int it = g_set.order[pos];
    const int y = 6 + pos * 37;
    const bool hid = hidden(it);
    text(s_rows, NAME_SHORT[it], &lv_font_tr_14, hid ? 0x666666 : C_WHITE, 122, y + 7, 90);
    for (int k = 0; k < 2; k++) {
      const bool ok = k ? pos < IT_COUNT - 1 : pos > 0;
      lv_obj_t *b = box(s_rows, k ? 252 : 214, y, 34, 32, ok ? C_BLUE : 0x222222, 9);
      lv_obj_t *l = lv_label_create(b);
      lv_label_set_text(l, k ? LV_SYMBOL_DOWN : LV_SYMBOL_UP);
      lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
      lv_obj_set_style_text_color(l, lv_color_hex(ok ? C_WHITE : 0x555555), 0);
      lv_obj_center(l);
      if (ok) clickable(b, move_cb, pos * 2 + k);
    }
    lv_obj_t *eye = box(s_rows, 284, y, 34, 32, 0, 0, false);
    clickable(eye, eye_cb, it);
    lv_obj_t *el = lv_label_create(eye);
    lv_label_set_text(el, hid ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN);
    lv_obj_set_style_text_font(el, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(el, lv_color_hex(hid ? 0x666666 : C_WHITE), 0);
    lv_obj_center(el);
  }
}

static void build_tab_layout(lv_obj_t *tab) {
  static const char *LAY[4] = T_LAYOUTS;
  memset(s_cardIcon, 0, sizeof(s_cardIcon));
  for (int k = 0; k < 4; k++) {
    s_card[k] = box(tab, 8, 4 + k * 38, 100, 34, C_CARD, 10);
    clickable(s_card[k], card_cb, k);
    lv_obj_t *ic = box(s_card[k], 12, 9, 26, 18, 0, 0, false);
    const int lay = CARD_LAYOUT[k];
    if (lay == 4) for (int r = 0; r < 4; r++) s_cardIcon[k][r] = box(ic, (r % 2) * 13, (r / 2) * 9, 11, 7, 0x666666, 2);
    if (lay == 3) for (int r = 0; r < 3; r++) s_cardIcon[k][r] = box(ic, r * 9, 0, 7, 16, 0x666666, 2);
    if (lay == 2) for (int r = 0; r < 2; r++) s_cardIcon[k][r] = box(ic, r * 13, 0, 11, 16, 0x666666, 2);
    if (lay == 1) s_cardIcon[k][0] = box(ic, 0, 0, 24, 16, 0x666666, 2);
    s_cardLbl[k] = text(s_card[k], LAY[k], &lv_font_tr_14, C_GREY, 48, 8, 50);
  }
  style_cards();
  s_rows = box(tab, 0, 0, 320, 168, 0, 0, false);
  rows_rebuild();
}

// ---- EKRAN sekmesi ----
static void slider_cb(lv_event_t *e) {
  g_set.brightness = lv_slider_get_value(s_slider);
  apply_backlight();
  if (lv_event_get_code(e) == LV_EVENT_RELEASED) settings_save();
}

static void style_slider() {
  const bool dis = g_set.autoBright;
  lv_obj_set_style_opa(s_slider, dis ? LV_OPA_40 : LV_OPA_COVER, 0);
}

static void auto_cb(lv_event_t *) {
  g_set.autoBright = lv_obj_has_state(s_switch, LV_STATE_CHECKED);
  s_lastBl = -1;
  apply_backlight();
  settings_save();
  style_slider();
}

static void off_cb(lv_event_t *) {
  const uint32_t id = lv_btnmatrix_get_selected_btn(s_btnm);
  if (id < 4) { g_set.offIdx = id; settings_save(); }
}

static void flip_cb(lv_event_t *e) {
  g_set.flip = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
  settings_save();
  hal_set_flip(g_set.flip);
}

static lv_obj_t *make_switch(lv_obj_t *tab, int y, bool on, lv_event_cb_t cb) {
  lv_obj_t *sw = lv_switch_create(tab);
  lv_obj_set_pos(sw, 262, y);
  lv_obj_set_size(sw, 46, 24);
  lv_obj_set_style_bg_color(sw, lv_color_hex(0x444444), LV_PART_MAIN);
  lv_obj_set_style_bg_color(sw, lv_color_hex(C_GREEN), (lv_style_selector_t)(LV_PART_INDICATOR | LV_STATE_CHECKED));
  lv_obj_set_style_bg_color(sw, lv_color_hex(C_WHITE), LV_PART_KNOB);
  if (on) lv_obj_add_state(sw, LV_STATE_CHECKED);
  lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, nullptr);
  return sw;
}

static void build_tab_screen(lv_obj_t *tab) {
  static const char *OFF4[4] = T_OFF_OPTS;
  static const char *OPTS[5];
  for (int i = 0; i < 4; i++) OPTS[i] = OFF4[i];
  OPTS[4] = "";

  text(tab, T_BRIGHT, &lv_font_tr_14, C_WHITE, 10, 4);
  s_slider = lv_slider_create(tab);
  lv_obj_set_pos(s_slider, 122, 13);
  lv_obj_set_size(s_slider, 186, 6);
  lv_slider_set_range(s_slider, 5, 100);
  lv_slider_set_value(s_slider, g_set.brightness, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(s_slider, lv_color_hex(0x333333), LV_PART_MAIN);
  lv_obj_set_style_bg_color(s_slider, lv_color_hex(C_BLUE), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(s_slider, lv_color_hex(C_WHITE), LV_PART_KNOB);
  lv_obj_set_style_pad_all(s_slider, 6, LV_PART_KNOB);
  lv_obj_add_event_cb(s_slider, slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(s_slider, slider_cb, LV_EVENT_RELEASED, nullptr);

  text(tab, T_AUTO, &lv_font_tr_14, C_WHITE, 10, 30);
  text(tab, T_AUTO_SUB, &lv_font_tr_12, C_GREY, 10, 48);
  s_switch = make_switch(tab, 34, g_set.autoBright, auto_cb);
  style_slider();

  text(tab, T_FLIP, &lv_font_tr_14, C_WHITE, 10, 74);
  make_switch(tab, 70, g_set.flip, flip_cb);

  text(tab, T_SCREEN_OFF, &lv_font_tr_14, C_WHITE, 10, 102);
  s_btnm = lv_btnmatrix_create(tab);
  lv_btnmatrix_set_map(s_btnm, OPTS);
  lv_obj_set_pos(s_btnm, 8, 124);
  lv_obj_set_size(s_btnm, 304, 30);
  lv_btnmatrix_set_btn_ctrl_all(s_btnm, LV_BTNMATRIX_CTRL_CHECKABLE);
  lv_btnmatrix_set_one_checked(s_btnm, true);
  lv_btnmatrix_set_btn_ctrl(s_btnm, g_set.offIdx, LV_BTNMATRIX_CTRL_CHECKED);
  lv_obj_set_style_bg_opa(s_btnm, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(s_btnm, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(s_btnm, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_column(s_btnm, 3, LV_PART_MAIN);
  lv_obj_set_style_bg_color(s_btnm, lv_color_hex(C_CARD), LV_PART_ITEMS);
  lv_obj_set_style_bg_color(s_btnm, lv_color_hex(C_BLUE), (lv_style_selector_t)(LV_PART_ITEMS | LV_STATE_CHECKED));
  lv_obj_set_style_border_width(s_btnm, 0, LV_PART_ITEMS);
  lv_obj_set_style_shadow_width(s_btnm, 0, LV_PART_ITEMS);
  lv_obj_set_style_radius(s_btnm, 8, LV_PART_ITEMS);
  lv_obj_set_style_text_font(s_btnm, &lv_font_tr_12, LV_PART_ITEMS);
  lv_obj_set_style_text_color(s_btnm, lv_color_hex(C_GREY), LV_PART_ITEMS);
  lv_obj_set_style_text_color(s_btnm, lv_color_hex(C_WHITE), (lv_style_selector_t)(LV_PART_ITEMS | LV_STATE_CHECKED));
  lv_obj_add_event_cb(s_btnm, off_cb, LV_EVENT_VALUE_CHANGED, nullptr);
}

// ---- DURUM sekmesi ----
static uint32_t rssi_color(int8_t r) { return r > -65 ? C_GREEN : r > -78 ? C_AMBER : C_RED; }

static void status_update() {
  char b[24];
  uint32_t age;
  const uint8_t down = now_link_down_mask();
  for (int k = 0; k < 2; k++) {          // 0 = masa dugumu, 1 = yatak dugumu
    const int8_t r = now_rssi(k, &age);
    if (r && age < 30000) {
      snprintf(b, sizeof(b), "%d dBm", r);
      lv_label_set_text(s_stVal[k], b);
      lv_obj_set_style_text_color(s_stVal[k], lv_color_hex(rssi_color(r)), 0);
    } else {
      const bool dn = down & (1 << k);
      lv_label_set_text(s_stVal[k], dn ? T_ST_NONE : "--");
      lv_obj_set_style_text_color(s_stVal[k], lv_color_hex(dn ? C_RED : C_GREY), 0);
    }
  }
  snprintf(b, sizeof(b), "%d", now_channel());
  lv_label_set_text(s_stVal[2], b);
}

static void status_tick(lv_timer_t *) {
  static int n = 0;
  status_update();
  if (++n % 4 == 0) now_query();         // 2 sn'de bir taze sinyal orneği
}

static void cal_cb(lv_event_t *) { hal_request_calibration(); }

static void build_tab_status(lv_obj_t *tab) {
  static const char *NAMES[3] = {T_ST_DESK, T_ST_BED, T_ST_CHANNEL};
  for (int i = 0; i < 3; i++) {
    text(tab, NAMES[i], &lv_font_tr_14, C_WHITE, 10, 12 + i * 28);
    s_stVal[i] = text(tab, "--", &lv_font_tr_14, C_WHITE, 160, 12 + i * 28, 148, LV_TEXT_ALIGN_RIGHT);
  }
  lv_obj_t *b = box(tab, 8, 112, 200, 38, C_BLUE, 10);
  clickable(b, cal_cb, 0);
  lv_obj_t *bl = text(b, T_CAL, &lv_font_tr_14, C_WHITE, 0, 0);
  lv_obj_center(bl);
  text(tab, T_VERSION, &lv_font_tr_12, C_GREY, 240, 123, 70, LV_TEXT_ALIGN_RIGHT);
  status_update();
}

// ---- sekmeler ----
static void select_tab(int t) {
  s_tab = t;
  for (int i = 0; i < 3; i++) {
    lv_obj_set_style_bg_color(s_tabBtn[i], lv_color_hex(i == t ? C_BLUE : C_CARD), 0);
    lv_obj_set_style_text_color(s_tabLbl[i], lv_color_hex(i == t ? C_WHITE : C_GREY), 0);
    if (i == t) lv_obj_clear_flag(s_tabBox[i], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_tabBox[i], LV_OBJ_FLAG_HIDDEN);
  }
  const bool st = (t == 2);
  now_rssi_enable(st);                   // sinyal olcumu sadece durum sekmesi acikken
  if (st && !s_stTimer) { s_stTimer = lv_timer_create(status_tick, 500, nullptr); now_query(); }
  if (!st && s_stTimer) { lv_timer_del(s_stTimer); s_stTimer = nullptr; }
}

static void tab_cb(lv_event_t *e) { select_tab((int)(intptr_t)lv_event_get_user_data(e)); }

#ifdef UI_DEBUG
#define MEMLOG(tag) Serial.printf("[mem] %-14s free=%u max=%u min=%u\n", tag, ESP.getFreeHeap(), ESP.getMaxAllocHeap(), ESP.getMinFreeHeap())
#else
#define MEMLOG(tag)
#endif

static void show_settings() {
  static const char *TABS[3] = T_TABS;
  MEMLOG("settings-baslar");
  s_set = lv_obj_create(nullptr);
  lv_obj_remove_style_all(s_set);
  lv_obj_set_size(s_set, SCREEN_W, SCREEN_H);
  lv_obj_set_style_bg_color(s_set, lv_color_hex(C_BG), 0);
  lv_obj_set_style_bg_opa(s_set, LV_OPA_COVER, 0);
  lv_obj_clear_flag(s_set, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *back = box(s_set, 0, 0, 56, 38, 0, 0, false);
  clickable(back, back_cb, 0);
  lv_obj_t *bl = lv_label_create(back);
  lv_label_set_text(bl, LV_SYMBOL_LEFT);
  lv_obj_set_style_text_font(bl, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(bl, lv_color_hex(C_WHITE), 0);
  lv_obj_align(bl, LV_ALIGN_CENTER, -4, 0);
  text(s_set, T_SETTINGS, &lv_font_tr_16, C_WHITE, 52, 8);

  for (int i = 0; i < 3; i++) {
    s_tabBtn[i] = box(s_set, 8 + i * 103, 40, 100, 28, C_CARD, 8);
    clickable(s_tabBtn[i], tab_cb, i);
    s_tabLbl[i] = text(s_tabBtn[i], TABS[i], &lv_font_tr_12, C_GREY, 0, 0);
    lv_obj_center(s_tabLbl[i]);
    s_tabBox[i] = box(s_set, 0, 72, SCREEN_W, 168, 0, 0, false);
  }
  MEMLOG("sekmeler-oncesi");
  build_tab_layout(s_tabBox[0]);
  MEMLOG("duzen-sekmesi");
  build_tab_screen(s_tabBox[1]);
  MEMLOG("ekran-sekmesi");
  build_tab_status(s_tabBox[2]);
  MEMLOG("durum-sekmesi");
  select_tab(0);
  lv_scr_load(s_set);
  MEMLOG("yuklendi");
}

static void leave_settings() {
  if (s_stTimer) { lv_timer_del(s_stTimer); s_stTimer = nullptr; }
  now_rssi_enable(false);
  lv_obj_t *oldMain = s_main, *oldSet = s_set;
  s_rows = nullptr;
  build_main();                          // duzen / sira / gizleme degismis olabilir
  lv_scr_load(s_main);
  s_set = nullptr;
  if (oldMain) lv_obj_del_async(oldMain);
  if (oldSet) lv_obj_del_async(oldSet);
}

// ---------------------------------------------------------------- baslatma
void ui_init(void) {
  hal_set_wake_cb(wake_cb);
  s_autoPct = hal_light_percent();
  build_main();
  lv_scr_load(s_main);
  lv_timer_create(now_tick, 200, nullptr);
  lv_timer_create(idle_tick, 200, nullptr);
  apply_backlight();
}

#ifdef UI_DEBUG
// gelistirme: seriden durum enjekte etmek icin
void ui_debug_set(int dev, bool on) {
  if (dev < 0 || dev > 2) return;
  s_on[dev] = on;
  s_conf[dev] = on;
  refresh_tiles();
}
#endif
