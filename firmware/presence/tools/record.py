#!/usr/bin/env python3
"""Varlik olcumu: kart /api/csi'yi 10 Hz'de kaydeder, yonergeli bir test akisi calistirir ve her bolumun istatistigini yazar.
Kullanim: python3 tools/record.py <ip> [plan.json]   (plan yoksa varsayilan test akisi kullanilir)"""
import json, sys, time, urllib.request, statistics as st

ip = sys.argv[1]
PLAN = [  # (etiket, sure sn, yonerge)
    ("sakin",        20, "HIC KIPIRDAMA (odada yalniz sen, kartin onunde 2 m uzakta otur)"),
    ("kapi-gec",     20, "Karttan 1 m onunden YURUYEREK gec, geri don, tekrarla (surekli)"),
    ("sakin2",       12, "HIC KIPIRDAMA"),
    ("el-salla",     15, "Kartin onunde elini SALLA"),
    ("sakin3",       12, "HIC KIPIRDAMA"),
    ("uzak-yuru",    20, "Odanin ULASABILDIGIN EN UZAK kosesinde yuru"),
    ("sakin4",       12, "HIC KIPIRDAMA"),
    ("oturan-hafif", 15, "Otur, sadece telefona bak / hafif kipirdan"),
]
def get():
    return json.loads(urllib.request.urlopen(f"http://{ip}/api/csi", timeout=1.5).read().decode())

rows = []
t0 = time.time()
print(f"Baslangic: {time.strftime('%H:%M:%S')}  (toplam {sum(p[1] for p in PLAN)} sn)\n", flush=True)
for label, dur, msg in PLAN:
    print(f">>> {label.upper()} ({dur} sn): {msg}", flush=True)
    end = time.time() + dur
    while time.time() < end:
        try:
            d = get(); d["label"] = label; d["t"] = time.time() - t0; rows.append(d)
        except Exception:
            pass
        time.sleep(0.1)
json.dump(rows, open("/tmp/presence_record.json", "w"))
print("\n=== SONUC ===")
for k in ("old", "win", "corr"):
    print(f"\n{k}:")
    allq = [r[k] for r in rows if r["label"].startswith("sakin")]
    qmax = max(allq) if allq else 0
    qmean = st.mean(allq) if allq else 0
    print(f"  sakin (hepsi)    ort={qmean:.4f} max={qmax:.4f}")
    for label, _, _ in PLAN:
        v = [r[k] for r in rows if r["label"] == label]
        if not v or label.startswith("sakin"): continue
        v.sort(); p50 = v[len(v)//2]; p90 = v[int(len(v)*0.9)]
        hit = sum(1 for x in v if x > qmax * 1.2) / len(v) * 100
        print(f"  {label:13s} ort={st.mean(v):.4f} medyan={p50:.4f} p90={p90:.4f} max={max(v):.4f}  | sakin_max*1.2 ustu: %{hit:.0f}")
rates = [r["rate"] for r in rows]
print(f"\npaket/sn ort={st.mean(rates):.0f}  atlanan={rows[-1]['skipped']}")
