// Bilgisayarda calistir: c++ -std=c++17 -x c++ -I../shared test_role_logic.c -o /tmp/trl && /tmp/trl
#include <stdio.h>
#include "role_logic.h"
static int bad = 0;
static void expect(const char *name, bool got, bool want) { if (got != want) { printf("FAIL %s\n", name); bad++; } else printf("ok   %s\n", name); }
static bool near(float a, float b) { return fabsf(a - b) < 0.01f; }

// 10 ms adimlarla pin dizisini calistir, hareket durumunu zaman cizelgesi olarak dondur
struct Run { PirCfg c; PirState s; uint32_t t = 100; int changes = 0; };
static void hold(Run &r, bool high, uint32_t ms) { for (uint32_t i = 0; i < ms; i += 10) { r.t += 10; if (pir_update(r.c, r.s, high, r.t)) r.changes++; } }

int main() {
  { Run r; r.c = {5, 5, 0, false};                       // onay 50 ms, tutma 5 sn
    hold(r, true, 30);  expect("pir: 30 ms'lik parazit yok sayilir", r.s.motion, false);
    hold(r, false, 100); expect("pir: parazit sonrasi hareket yok", r.s.motion || r.changes > 0, false);
    hold(r, true, 100); expect("pir: 100 ms darbe hareket sayilir", r.s.motion, true);
    hold(r, false, 4000); expect("pir: tutma suresi icinde hala hareket var", r.s.motion, true);
    hold(r, false, 1500); expect("pir: tutma bitince hareket yok", r.s.motion, false);
    expect("pir: iki degisim (var, yok)", r.changes == 2, true); }
  { Run r; r.c = {2, 5, 0, false};
    hold(r, true, 100); hold(r, false, 1500); hold(r, true, 100); hold(r, false, 1000);
    expect("pir: tutma suresi icinde yeni hareket zamani uzatir", r.s.motion, true);
    hold(r, false, 1500); expect("pir: sonunda biter", r.s.motion, false); }
  { Run r; r.c = {0, 5, 3, false};                       // tutma 0, bekleme 3 sn
    hold(r, true, 100); hold(r, false, 100); expect("pir: tutma 0: hemen biter", r.s.motion, false);
    hold(r, true, 200); expect("pir: bekleme suresinde yeniden tetiklenmez", r.s.motion, false);
    hold(r, false, 3000); hold(r, true, 200); expect("pir: bekleme sonrasi tetiklenir", r.s.motion, true); }
  { Run r; r.c = {0, 0, 0, true};                        // ters: LOW = hareket
    hold(r, false, 20); expect("pir ters: LOW hareket", r.s.motion, true);
    hold(r, true, 20);  expect("pir ters: HIGH bosta", r.s.motion, false); }
  // servo
  { float cur = 0; for (int i = 0; i < 100; i++) cur = servo_step(cur, 90, 30, 0.02f);   // 30 derece/sn, 2 sn
    expect("servo: 2 sn'de 60 derece", near(cur, 60), true);
    for (int i = 0; i < 100; i++) cur = servo_step(cur, 90, 30, 0.02f);
    expect("servo: hedefi asmaz", near(cur, 90), true);
    expect("servo: geri yon", near(servo_step(90, 0, 30, 0.1f), 87), true);
    expect("servo: hiz 0 aninda", near(servo_step(0, 180, 0, 0.02f), 180), true); }
  expect("servo: sinir alt", servo_clamp(-5, 10, 170) == 10, true);
  expect("servo: sinir ust", servo_clamp(200, 10, 170) == 170, true);
  expect("servo: ici", servo_clamp(90, 10, 170) == 90, true);
  expect("servo: 0 derece 544 us", servo_pulse_us(0, false) == 544, true);
  expect("servo: 180 derece 2400 us", servo_pulse_us(180, false) == 2400, true);
  expect("servo: ters 0 derece = 2400 us", servo_pulse_us(0, true) == 2400, true);
  expect("servo: 90 derece ~1472 us", servo_pulse_us(90, false) == 1472, true);
  expect("servo: birakma suresi dolmadi", !servo_should_release(10, 900, 0), true);
  expect("servo: birakma suresi doldu", servo_should_release(10, 1000, 0), true);
  expect("servo: asla birakma (0)", !servo_should_release(0, 99999, 0), true);
  if (!bad) puts("hepsi tamam");
  return bad;
}
