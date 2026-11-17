#!/usr/bin/env python3
"""Uygulama imaji (firmware.bin) derleme klasorundeki bolum tablosunun OTA yuvasina sigiyor mu?

Kullanim:  check_app_fit.py <derleme-klasoru> [...] [--warn-only]
  <derleme-klasoru> icinde partitions.bin ve firmware.bin olmali (ornek: firmware/node/.pio/build/esp32c3)
Iki OTA yuvasi (ota_0, ota_1) varsa imaj KUCUK olan yuvaya sigmali: kablosuz guncelleme iki yuvayi da kullanir.
Cikis kodu: 0 hepsi sigiyor, 1 en az biri sigmiyor (--warn-only ile hep 0, GitHub uyari satiri yazar).
Plan: docs/plan-dugum-ota.md
"""
import os
import struct
import sys

SUBTYPE_FACTORY = 0x00
SUBTYPE_OTA_MIN, SUBTYPE_OTA_MAX = 0x10, 0x1F


def read_partitions(data):
    """partitions.bin -> [(ad, tip, altTip, ofset, boyut)]"""
    out = []
    for i in range(0, len(data) - 31, 32):
        e = data[i:i + 32]
        if e[:2] != b"\xaa\x50":      # 0xEBEB = MD5 satiri, 0xFFFF = bos: tablo bitti
            break
        ptype, sub, off, size = struct.unpack("<BBII", e[2:12])
        name = e[12:28].split(b"\0")[0].decode(errors="replace")
        out.append((name, ptype, sub, off, size))
    return out


def check(build_dir):
    pt = os.path.join(build_dir, "partitions.bin")
    fw = os.path.join(build_dir, "firmware.bin")
    with open(pt, "rb") as f:
        parts = read_partitions(f.read())
    image = os.path.getsize(fw)
    apps = [p for p in parts if p[1] == 0 and (p[2] == SUBTYPE_FACTORY or SUBTYPE_OTA_MIN <= p[2] <= SUBTYPE_OTA_MAX)]
    ota = [p for p in apps if p[2] != SUBTYPE_FACTORY]
    if not ota:
        return False, image, 0, "OTA yuvasi yok (ota_0/ota_1 bolumu tanimli degil)"
    slot = min(p[4] for p in ota)
    if len(ota) < 2:
        return False, image, slot, "yalnizca %d OTA yuvasi var, 2 gerekli" % len(ota)
    if image > slot:
        return False, image, slot, "imaj yuvadan %d bayt buyuk" % (image - slot)
    return True, image, slot, "bos kalan %d bayt" % (slot - image)


def main(argv):
    warn_only = "--warn-only" in argv
    dirs = [a for a in argv if not a.startswith("--")]
    if not dirs:
        print(__doc__)
        return 2
    bad = 0
    for d in dirs:
        name = os.path.basename(os.path.normpath(d))
        ok, image, slot, msg = check(d)
        line = "%-22s imaj %8d B  yuva %8d B  %s" % (name, image, slot, msg)
        if ok:
            print("ok    " + line)
        else:
            bad += 1
            print("SIGMIYOR " + line)
            if warn_only:
                print("::warning title=OTA yuvasina sigmiyor (%s)::%s" % (name, msg))
    return 0 if (bad == 0 or warn_only) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
