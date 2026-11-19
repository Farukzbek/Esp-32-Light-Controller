// Bilgisayarda calistir: c++ -std=c++17 -x c++ -I../shared test_schedule.c -o /tmp/tsc && /tmp/tsc
#include <stdio.h>
#include "schedule.h"
static int bad = 0;
static void expect(const char *name, bool ok) { if (!ok) { printf("FAIL %s\n", name); bad++; } else printf("ok   %s\n", name); }
// 2026-11-16 Pazartesi: 1970-01-01'den 20773 gun sonra
static const int32_t MON = 20773;
enum { PZT = 0, SAL, CAR, PER, CUM, CMT, PAZ };
int main() {
  expect("takvim: 2026-11-16 Pazartesi", sched_wday(MON) == PZT);
  expect("takvim: +6 Pazar", sched_wday(MON + 6) == PAZ);
  expect("takvim: -1 Pazar", sched_wday(MON - 1) == PAZ);
  expect("takvim: 1970-01-01 Persembe", sched_wday(0) == PER);
  // ayristirma
  SchedRule r[SCHED_MAX]; int n = 0;
  expect("ayristir: tek kural", sched_parse("1:1:15:450:127:90", r, SCHED_MAX, &n) && n == 1 && r[0].minute == 450 && r[0].devices == 15 && r[0].angle == 90);
  expect("ayristir: iki kural", sched_parse("1:1:8:450:63:90;0:2:1:1380:127:0", r, SCHED_MAX, &n) && n == 2 && !r[1].en && r[1].action == SCHED_OFF);
  expect("ayristir: bos metin = kural yok", sched_parse("", r, SCHED_MAX, &n) && n == 0);
  expect("reddet: 5 alan", !sched_parse("1:1:15:450:127", r, SCHED_MAX, &n));
  expect("reddet: 7 alan", !sched_parse("1:1:15:450:127:90:1", r, SCHED_MAX, &n));
  expect("reddet: dakika 1440", !sched_parse("1:1:15:1440:127:90", r, SCHED_MAX, &n));
  expect("reddet: gun 0", !sched_parse("1:1:15:450:0:90", r, SCHED_MAX, &n));
  expect("reddet: gun 128", !sched_parse("1:1:15:450:128:90", r, SCHED_MAX, &n));
  expect("reddet: eylem 4", !sched_parse("1:4:15:450:127:90", r, SCHED_MAX, &n));
  expect("reddet: aci 181", !sched_parse("1:3:15:450:127:181", r, SCHED_MAX, &n));
  expect("reddet: cihaz 0", !sched_parse("1:1:0:450:127:90", r, SCHED_MAX, &n));
  expect("reddet: harf", !sched_parse("1:1:x:450:127:90", r, SCHED_MAX, &n));
  expect("reddet: bos alan", !sched_parse("1:1::450:127:90", r, SCHED_MAX, &n));
  expect("reddet: 17 kural", !sched_parse("1:1:1:0:127:0;1:1:1:1:127:0;1:1:1:2:127:0;1:1:1:3:127:0;1:1:1:4:127:0;1:1:1:5:127:0;1:1:1:6:127:0;1:1:1:7:127:0;1:1:1:8:127:0;1:1:1:9:127:0;1:1:1:10:127:0;1:1:1:11:127:0;1:1:1:12:127:0;1:1:1:13:127:0;1:1:1:14:127:0;1:1:1:15:127:0;1:1:1:16:127:0", r, SCHED_MAX, &n));
  n = 5; expect("reddedilince cikti degismez", !sched_parse("bozuk", r, SCHED_MAX, &n) && n == 5);
  // metne cevir <-> ayristir
  { SchedRule a[2] = {{true, SCHED_ANGLE, 8, 450, 63, 120}, {false, SCHED_OFF, 3, 1380, 127, 0}}; char buf[200]; SchedRule b[4]; int m = 0;
    expect("yaz-oku: gidis donus", sched_to_string(a, 2, buf, sizeof(buf)) && sched_parse(buf, b, 4, &m) && m == 2 && b[0].angle == 120 && b[0].days == 63 && b[1].minute == 1380 && !b[1].en);
    char small[10]; expect("yaz: tasma reddedilir", !sched_to_string(a, 2, small, sizeof(small))); }
  // calistirma
  { SchedRule a[1] = {{true, SCHED_ON, 1, 450, 127, 0}}; SchedState st;                  // 07:30 her gun
    expect("tetik: 07:29 degil", sched_poll(a, 1, st, MON, 449, 0) == 0);
    expect("tetik: 07:30 evet", sched_poll(a, 1, st, MON, 450, 0) == 1);
    expect("tetik: ayni dakikada ikinci kez degil", sched_poll(a, 1, st, MON, 450, 0) == 0);
    expect("tetik: 07:31 degil", sched_poll(a, 1, st, MON, 451, 0) == 0);
    expect("tetik: ertesi gun yine", sched_poll(a, 1, st, MON + 1, 450, 0) == 1); }
  { SchedRule a[1] = {{true, SCHED_ON, 1, 450, (uint8_t)(127 & ~(1 << PAZ)), 0}}; SchedState st;   // Pazar haric
    expect("gun: Cumartesi calisir", sched_poll(a, 1, st, MON + 5, 450, 0) == 1);
    expect("gun: Pazar calismaz", sched_poll(a, 1, st, MON + 6, 450, 0) == 0);
    expect("gun: Pazartesi calisir", sched_poll(a, 1, st, MON + 7, 450, 0) == 1); }
  { SchedRule a[1] = {{true, SCHED_ON, 1, 450, 127, 0}}; SchedState st;
    expect("kacirilan atlanir (yakalama 0)", sched_poll(a, 1, st, MON, 455, 0) == 0); }
  { SchedRule a[1] = {{true, SCHED_ON, 1, 450, 127, 0}}; SchedState st;
    expect("yakalama 10 dk: 7 dk gec yine calisir", sched_poll(a, 1, st, MON, 457, 10) == 1);
    expect("yakalama: bir kez", sched_poll(a, 1, st, MON, 458, 10) == 0);
    SchedState st2; expect("yakalama: 11 dk gec calismaz", sched_poll(a, 1, st2, MON, 461, 10) == 0); }
  { SchedRule a[1] = {{true, SCHED_ON, 1, 1439, 127, 0}}; SchedState st;                  // 23:59, gece yarisi gecisi
    expect("gece yarisi: 23:59 calisir", sched_poll(a, 1, st, MON, 1439, 5) == 1);
    expect("gece yarisi: 00:01 ertesi gun ayni tetigi tekrarlamaz", sched_poll(a, 1, st, MON + 1, 1, 5) == 0); }
  { SchedRule a[3] = {{true, SCHED_ON, 1, 450, 127, 0}, {false, SCHED_ON, 1, 450, 127, 0}, {true, SCHED_OFF, 2, 450, 127, 0}}; SchedState st;
    expect("kapali kural atlanir, ayni dakikada iki kural", sched_poll(a, 3, st, MON, 450, 0) == 0b101); }
  if (!bad) puts("hepsi tamam");
  return bad;
}
