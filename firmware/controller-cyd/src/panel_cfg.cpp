#include <Preferences.h>
#include "panel_cfg.h"

PanelSettings g_set = {4, {IT_MASA, IT_YATAK, IT_LED, IT_ALL}, 0, 70, false, 1, false};
const uint16_t OFF_SECS[4] = {30, 60, 300, 0};

void settings_load(void) {
  Preferences p;
  if (!p.begin("panel", false)) return;   // yazilabilir ac: ilk acilista alan olusur, "NOT_FOUND" uyarisi cikmaz
  PanelSettings s = g_set;
  const uint8_t lay = p.getUChar("layout", s.layout);
  if (lay >= 1 && lay <= 4) s.layout = lay;
  uint8_t o[IT_COUNT];
  if (p.getBytes("order", o, IT_COUNT) == IT_COUNT) {          // gecerli bir permutasyon degilse varsayilan kalir
    bool seen[IT_COUNT] = {false}, ok = true;
    for (int i = 0; i < IT_COUNT; i++) { if (o[i] >= IT_COUNT || seen[o[i]]) ok = false; else seen[o[i]] = true; }
    if (ok) for (int i = 0; i < IT_COUNT; i++) s.order[i] = o[i];
  }
  s.hiddenMask = p.getUChar("hidden", s.hiddenMask) & ((1 << IT_COUNT) - 1);
  if (s.hiddenMask == (1 << IT_COUNT) - 1) s.hiddenMask = 0;   // hepsi gizli olamaz
  const uint8_t br = p.getUChar("bright", s.brightness);
  if (br >= 5 && br <= 100) s.brightness = br;
  s.autoBright = p.getBool("auto", s.autoBright);
  const uint8_t off = p.getUChar("off", s.offIdx);
  if (off < 4) s.offIdx = off;
  s.flip = p.getBool("flip", s.flip);
  p.end();
  g_set = s;
}

void settings_save(void) {
  Preferences p;
  if (!p.begin("panel", false)) return;
  p.putUChar("layout", g_set.layout);
  p.putBytes("order", g_set.order, IT_COUNT);
  p.putUChar("hidden", g_set.hiddenMask);
  p.putUChar("bright", g_set.brightness);
  p.putBool("auto", g_set.autoBright);
  p.putUChar("off", g_set.offIdx);
  p.putBool("flip", g_set.flip);
  p.end();
}
