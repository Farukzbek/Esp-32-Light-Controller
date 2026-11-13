#pragma once
#include <stdint.h>
#include <stdbool.h>

// Panel ayarlari (NVS'de kalici): ana ekran duzeni, karo sirasi, gizlenen karolar, parlaklik, ekran kapanma.
enum { IT_MASA, IT_YATAK, IT_LED, IT_ALL, IT_COUNT };   // karolar; ilk ucu gercek cihaz, IT_ALL = tum isiklar

struct PanelSettings {
  uint8_t layout;            // 4, 3, 2 ya da 1: ekranda ayni anda kac karo
  uint8_t order[IT_COUNT];   // karo sirasi (IT_* degerlerinin bir permutasyonu), tum duzenlerde ortak
  uint8_t hiddenMask;        // bit i = IT_i karosu gizli
  uint8_t brightness;        // 5-100 (otomatik kapaliyken)
  bool autoBright;           // isik sensorune gore
  uint8_t offIdx;            // OFF_SECS dizisinde indeks
  bool flip;                 // ekran 180 derece cevrik (panel ters monte edildiyse)
};

extern PanelSettings g_set;
extern const uint16_t OFF_SECS[4];   // 30, 60, 300, 0 (0 = hic kapanmaz)

void settings_load(void);
void settings_save(void);
