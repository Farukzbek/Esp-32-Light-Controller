#!/usr/bin/env python3
"""check_app_fit.py ve make_ota_manifest.py testleri. Calistir: python3 firmware/tools/test_ota_tools.py"""
import hashlib
import json
import os
import struct
import tempfile
import unittest

import check_app_fit as fit
import make_ota_manifest as mm


def entry(name, ptype, sub, off, size):
    return b"\xaa\x50" + struct.pack("<BBII", ptype, sub, off, size) + name.encode().ljust(16, b"\0") + b"\0" * 4


def table(*entries):
    return b"".join(entries) + b"\xeb\xeb" + b"\xff" * 30


class FitTest(unittest.TestCase):
    def build(self, parts, image_size):
        d = tempfile.mkdtemp()
        with open(os.path.join(d, "partitions.bin"), "wb") as f:
            f.write(table(*parts))
        with open(os.path.join(d, "firmware.bin"), "wb") as f:
            f.write(b"\0" * image_size)
        return d

    DEFAULT = [entry("nvs", 1, 2, 0x9000, 0x5000), entry("otadata", 1, 0, 0xe000, 0x2000),
               entry("app0", 0, 0x10, 0x10000, 0x140000), entry("app1", 0, 0x11, 0x150000, 0x140000)]

    def test_fits(self):
        ok, image, slot, _ = fit.check(self.build(self.DEFAULT, 1209024))
        self.assertTrue(ok); self.assertEqual(slot, 0x140000)

    def test_too_big(self):          # gercek durum: C3 imaji 1.280 KB'lik yuvaya sigmiyor
        ok, image, slot, msg = fit.check(self.build(self.DEFAULT, 1333472))
        self.assertFalse(ok); self.assertIn("buyuk", msg)

    def test_exact(self):
        ok, *_ = fit.check(self.build(self.DEFAULT, 0x140000)); self.assertTrue(ok)

    def test_smaller_slot_counts(self):
        parts = [entry("app0", 0, 0x10, 0x10000, 0x200000), entry("app1", 0, 0x11, 0x210000, 0x100000)]
        ok, _, slot, _ = fit.check(self.build(parts, 0x150000))
        self.assertFalse(ok); self.assertEqual(slot, 0x100000)

    def test_single_slot_fails(self):
        parts = [entry("factory", 0, 0x00, 0x10000, 0x300000)]
        ok, _, _, msg = fit.check(self.build(parts, 1000))
        self.assertFalse(ok); self.assertIn("OTA", msg)


class ManifestTest(unittest.TestCase):
    def test_hash_and_size(self):
        data = os.urandom(5000)
        d = tempfile.mkdtemp(); p = os.path.join(d, "x.bin")
        open(p, "wb").write(data)
        m = mm.manifest(p, "0.3.0", "esp32c3.ota.bin")
        self.assertEqual(m["size"], 5000)
        self.assertEqual(m["sha256"], hashlib.sha256(data).hexdigest())
        self.assertEqual(m["file"], "esp32c3.ota.bin"); self.assertEqual(m["version"], "0.3.0")

    def test_cli_writes_json(self):
        d = tempfile.mkdtemp(); p = os.path.join(d, "x.bin"); o = os.path.join(d, "o.json")
        open(p, "wb").write(b"abc")
        self.assertEqual(mm.main([p, "1.0", o, "--file", "esp.ota.bin"]), 0)
        self.assertEqual(json.load(open(o))["sha256"], hashlib.sha256(b"abc").hexdigest())


if __name__ == "__main__":
    unittest.main()
