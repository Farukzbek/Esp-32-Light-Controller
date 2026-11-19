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
  expect("ters: ac -> kapat", reenter_opposite(1) == 2);
  expect("ters: kapat -> ac", reenter_opposite(2) == 1);
  expect("ters: cevir -> cevir", reenter_opposite(0) == 0);
  if (!bad) puts("hepsi tamam");
  return bad;
}
