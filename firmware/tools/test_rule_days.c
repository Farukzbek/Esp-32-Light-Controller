// Bilgisayarda calistir: c++ -std=c++17 -x c++ -I../shared test_rule_days.c -o /tmp/trd && /tmp/trd
#include <stdio.h>
#include "rule_days.h"
static int bad = 0;
static void expect(const char *name, bool got, bool want) { if (got != want) { printf("FAIL %s\n", name); bad++; } else printf("ok   %s\n", name); }
enum { PAZ = 0, PZT = 1, SAL = 2, CAR = 3, PER = 4, CUM = 5, CMT = 6 };    // tm_wday
#define DAY(n) (1 << (n))                                                   // 0 Pzt ... 6 Paz
int main() {
  const uint8_t ALL = RULE_ALL_DAYS;
  const uint8_t NO_SUNDAY = ALL & ~DAY(6);
  const uint8_t ONLY_SUNDAY = DAY(6);
  // sabah perdesi: 07:00-09:00, pazar hic acilmasin
  expect("pazar yasak: Pazar 08:00", rule_in_window(NO_SUNDAY, PAZ, 8 * 60, 7 * 60, 9 * 60), false);
  expect("pazar yasak: Pazartesi 08:00", rule_in_window(NO_SUNDAY, PZT, 8 * 60, 7 * 60, 9 * 60), true);
  expect("pazar yasak: Cumartesi 08:00", rule_in_window(NO_SUNDAY, CMT, 8 * 60, 7 * 60, 9 * 60), true);
  expect("saat disi", rule_in_window(NO_SUNDAY, PZT, 10 * 60, 7 * 60, 9 * 60), false);
  expect("bitis haric", rule_in_window(ALL, PZT, 9 * 60, 7 * 60, 9 * 60), false);
  expect("baslangic dahil", rule_in_window(ALL, PZT, 7 * 60, 7 * 60, 9 * 60), true);
  // gun boyu (bas == bit)
  expect("gun boyu her gun", rule_in_window(ALL, CAR, 3 * 60 + 33, 0, 0), true);
  expect("gun boyu sadece pazar: Pazar", rule_in_window(ONLY_SUNDAY, PAZ, 12 * 60, 5, 5), true);
  expect("gun boyu sadece pazar: Sali", rule_in_window(ONLY_SUNDAY, SAL, 12 * 60, 5, 5), false);
  // gece yarisini asan pencere: gun = baslangic gunu
  expect("gece: Pazar 23:00, pazar secili", rule_in_window(ONLY_SUNDAY, PAZ, 23 * 60, 22 * 60, 6 * 60), true);
  expect("gece: Pazartesi 02:00 (Pazar baslangicli), pazar secili", rule_in_window(ONLY_SUNDAY, PZT, 2 * 60, 22 * 60, 6 * 60), true);
  expect("gece: Pazartesi 23:00, pazar secili", rule_in_window(ONLY_SUNDAY, PZT, 23 * 60, 22 * 60, 6 * 60), false);
  expect("gece: Pazar 02:00 (Cumartesi baslangicli), pazar secili", rule_in_window(ONLY_SUNDAY, PAZ, 2 * 60, 22 * 60, 6 * 60), false);
  expect("gece: Pazar 02:00, cumartesi secili", rule_in_window(DAY(5), PAZ, 2 * 60, 22 * 60, 6 * 60), true);
  expect("gece: 06:00 haric", rule_in_window(ALL, PZT, 6 * 60, 22 * 60, 6 * 60), false);
  expect("hicbir gun secili degil", rule_in_window(0, PZT, 8 * 60, 7 * 60, 9 * 60), false);
  // hafta ici
  const uint8_t WEEKDAYS = DAY(0) | DAY(1) | DAY(2) | DAY(3) | DAY(4);
  for (int w = 0; w < 7; w++) expect(w == PAZ || w == CMT ? "hafta ici: hafta sonu kapali" : "hafta ici: acik", rule_in_window(WEEKDAYS, w, 8 * 60, 7 * 60, 9 * 60), !(w == PAZ || w == CMT));
  if (!bad) puts("hepsi tamam");
  return bad;
}
