#!/usr/bin/env python3
"""Dugum OTA bilgi dosyasi: kopru bunu okuyup imaji dogrular.

Kullanim:  make_ota_manifest.py <firmware.bin> <surum> <cikti.json> [--file esp32xx.ota.bin]
Cikti: {"version", "size", "sha256", "file"}
Plan: docs/plan-dugum-ota.md
"""
import hashlib
import json
import os
import sys


def manifest(path, version, file_name=None):
    with open(path, "rb") as f:
        data = f.read()
    return {
        "version": version,
        "size": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "file": file_name or os.path.basename(path),
    }


def main(argv):
    args = [a for a in argv if not a.startswith("--")]
    file_name = None
    if "--file" in argv:
        i = argv.index("--file")
        file_name = argv[i + 1]
        args = [a for a in args if a != file_name]
    if len(args) != 3:
        print(__doc__)
        return 2
    m = manifest(args[0], args[1], file_name)
    with open(args[2], "w") as f:
        json.dump(m, f, indent=2)
        f.write("\n")
    print("%s: %d bayt, sha256 %s" % (m["file"], m["size"], m["sha256"][:16] + "..."))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
