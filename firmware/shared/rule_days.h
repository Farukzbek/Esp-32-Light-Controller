#pragma once
#include <stdint.h>

// Otomasyon kurali: calisma gunleri + saat penceresi. Kapi dugumu (firmware/door) ve bilgisayar testi (tools/test_rule_days.c) kullanir.
// days: bit0 = Pazartesi ... bit6 = Pazar; 127 = her gun. tmWday: struct tm tm_wday (0 = Pazar ... 6 = Cumartesi).
// Gece yarisini asan pencerede (orn. 22:00-06:00) gun, pencerenin BASLADIGI gundur: Pazar secili ise Pazar 22:00 - Pazartesi 06:00 calisir.
#define RULE_ALL_DAYS 127

inline bool rule_in_window(uint8_t days, int tmWday, uint16_t minuteOfDay, uint16_t start, uint16_t end) {
  int day = (tmWday + 6) % 7;                          // Pzt = 0 ... Paz = 6
  bool inTime;
  if (start == end) inTime = true;                     // bas = bit: gun boyu
  else if (start < end) inTime = minuteOfDay >= start && minuteOfDay < end;
  else {
    inTime = minuteOfDay >= start || minuteOfDay < end;
    if (inTime && minuteOfDay < end) day = (day + 6) % 7;   // gece yarisindan sonraki kisim: pencere dun basladi
  }
  return inTime && ((days >> day) & 1);
}
