// Bilgisayarda calistir: c++ -std=c++17 -x c++ -I../shared test_reenter.c -o /tmp/tre && /tmp/tre
#include <stdio.h>
#include "reenter.h"
static int bad = 0;
static void expect(const char *name, bool ok) { if (!ok) { printf("FAIL %s\n", name); bad++; } else printf("ok   %s\n", name); }
int main() {
  { ReenterState s; expect("pencere yokken: kural normal", reenter_check(s, 1000, 60, 8) == RE_NONE); }
  { ReenterState s; reenter_arm(s, 2, 10000);
    expect("2 sn sonra: ayni ziyaret", reenter_check(s, 12000, 60, 8) == RE_SAME_VISIT);
    expect("ayni ziyaret pencereyi kapatmaz", s.armed);
    expect("30 sn sonra: gir-cik, ters", reenter_check(s, 40000, 60, 8) == RE_REVERSE && s.rule == 2);
    expect("tam sinirda (8 sn) ters", [] { ReenterState t; reenter_arm(t, 0, 5000); return reenter_check(t, 13000, 60, 8) == RE_REVERSE; }());
    expect("7,9 sn: henuz ayni ziyaret", [] { ReenterState t; reenter_arm(t, 0, 5000); return reenter_check(t, 12900, 60, 8) == RE_SAME_VISIT; }()); }
  { ReenterState s; reenter_arm(s, 0, 10000);
    expect("60 sn sinirinda hala ters", reenter_check(s, 70000, 60, 8) == RE_REVERSE);
    ReenterState t; reenter_arm(t, 0, 10000);
    expect("61 sn: pencere doldu, normal", reenter_check(t, 71000, 60, 8) == RE_NONE);
    expect("zaman asimi pencereyi temizler", !t.armed); }
  { ReenterState s; reenter_arm(s, 1, 10000); expect("ozellik kapali (0 sn)", reenter_check(s, 30000, 0, 8) == RE_NONE); }
  { ReenterState s; reenter_arm(s, 1, 0xFFFFFF00u);                      // millis tasmasi: armedAt tasmadan once, simdi tasmis
    expect("millis tasmasi: 20 sn sonra ters", reenter_check(s, 0xFFFFFF00u + 20000u, 60, 8) == RE_REVERSE); }
  { ReenterState s; reenter_arm(s, 3, 5000); reenter_clear(s); expect("temizlenince normal", reenter_check(s, 30000, 60, 8) == RE_NONE); }
  // sakinlesme: ters eylemden sonra art arda gelen hareketler yok sayilir (sure uzar ama ust sinir var)
  { ReenterState s; reenter_lock(s, 100000, 5000);
    expect("sakinlesme: 1 sn sonra hareket yok sayilir", reenter_locked(s, 101000, 5000, 10000));
    expect("sakinlesme: sure uzar (5 sn sonra hala)", reenter_locked(s, 105000, 5000, 10000));
    expect("sakinlesme: ust sinir (10 sn) icinde hala", reenter_locked(s, 109000, 5000, 10000));
    expect("sakinlesme: ust sinirdan sonra uzamaz, biter", !reenter_locked(s, 110500, 5000, 10000));
    expect("sakinlesme bitince kilit kalkar", !s.locked && !reenter_locked(s, 111000, 5000, 10000)); }
  { ReenterState s; reenter_lock(s, 0, 5000);
    expect("disarida oyalanma: surekli hareket olsa bile 10 sn'de biter", [&] { bool b = true; for (uint32_t t = 500; t < 9900; t += 500) b = b && reenter_locked(s, t, 5000, 10000); return b && !reenter_locked(s, 10100, 5000, 10000); }()); }
  { ReenterState s; reenter_lock(s, 0, 5000);
    expect("sessiz kalinca 5 sn'de biter", !reenter_locked(s, 5500, 5000, 10000)); }
  { ReenterState s; expect("kilit hic konmadiysa hareket serbest", !reenter_locked(s, 5000, 5000, 10000)); }
  { ReenterState s; reenter_lock(s, 0xFFFFFFF0u, 5000);
    expect("sakinlesme millis tasmasi", reenter_locked(s, 1000, 5000, 10000)); }
  { ReenterState s; reenter_arm(s, 0, 10000);
    expect("20 dk pencere: 19 dk sonra hala ters", reenter_check(s, 10000 + 19 * 60000u, 1200, 8) == RE_REVERSE);
    ReenterState t; reenter_arm(t, 0, 10000);
    expect("20 dk pencere: 20 dk 1 sn sonra normal", reenter_check(t, 10000 + 1201000u, 1200, 8) == RE_NONE); }
  // geri yukleme maskesi
  { RestoreState r; restore_set(r, 0b010, 1000);                                // yalniz yatak (yuva 1) aciktiydi
    expect("yalniz yatak hatirlanir", restore_take(r, 0b111, 5000, 1200000) == 0b010);
    expect("kullanilinca silinir", restore_take(r, 0b111, 6000, 1200000) == 0); }
  { RestoreState r; restore_set(r, 0b011, 1000);
    expect("masa+yatak: kural isiklariyla kesisim", restore_take(r, 0b110, 2000, 1200000) == 0b010); }
  { RestoreState r; restore_set(r, 0b001, 1000);
    expect("eski (20 dk+): yok sayilir, kural normal", restore_take(r, 0b111, 1000 + 1200001u, 1200000) == 0); }
  { RestoreState r; restore_set(r, 0b101, 0xFFFFFF00u);
    expect("millis tasmasi: hala taze", restore_take(r, 0b111, 100, 1200000) == 0b101); }
  { RestoreState r; restore_set(r, 0, 1000); expect("bos mask hatirlanmaz", !r.valid && restore_take(r, 0b111, 2000, 1200000) == 0); }
  { RestoreState r; expect("hic hatirlanmadiysa 0", restore_take(r, 0b111, 2000, 1200000) == 0); }
  expect("ters: ac -> kapat", reenter_opposite(1) == 2);
  expect("ters: kapat -> ac", reenter_opposite(2) == 1);
  expect("ters: cevir -> cevir", reenter_opposite(0) == 0);
  if (!bad) puts("hepsi tamam");
  return bad;
}
