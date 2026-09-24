#pragma once
#include <stdint.h>

// Kumanda (Waveshare AMOLED) <-> masa dugumu (hub) ve yatak dugumu ESP-NOW protokolu.
// Iki uc da BU dosyayi kullanir: mesaj boyutu (sizeof(NowMsg)) iki tarafta ayni olmak zorunda.

// Cihaza ozel ayarlar (MAC adresleri, ESP-NOW kanali, sifreleme parolasi) now_config.h icinde.
#if __has_include("now_config.h")
#include "now_config.h"
#else
#error "now_config.h yok: firmware/shared/now_config.example.h dosyasini now_config.h olarak kopyalayip doldur (bkz. docs/setup.md)"
#endif

enum : uint8_t { NOW_SET = 1, NOW_QUERY = 2, NOW_STATE = 3 };
enum : uint8_t { DEV_MASA = 0, DEV_YATAK = 1, DEV_LED = 2, DEV_COUNT = 3 };

// STATE mesajinda st[i] == NOW_UNKNOWN ise gonderen o cihazin sahibi degildir, degeri degistirme.
#define NOW_UNKNOWN 0xFF

struct __attribute__((packed)) NowMsg {
  uint8_t  type;   // NOW_SET / NOW_QUERY / NOW_STATE
  uint8_t  dev;    // SET: hangi cihaz
  uint8_t  val;    // SET: 0 / 1
  uint8_t  st[4];  // STATE: cihaz durumlari (masa, yatak, led, rezerve)
  uint16_t seq;    // istek numarasi; STATE yanitinda ayni numara, hub'in kendi bildirimlerinde 0
};
