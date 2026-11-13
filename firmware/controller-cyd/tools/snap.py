#!/usr/bin/env python3
"""cyd-debug yazilimiyla konusur: sanal dokunma ve ekran goruntusu (PNG).

Kullanim:  python3 tools/snap.py PORT ONEK "snap" "tap 276 15" "wait 1" "snap" ...
  snap        ekran goruntusu al  -> ONEK_1.png, ONEK_2.png, ...
  tap X Y     sanal dokunma (ekran koordinati, 320x240)
  drag X1 Y1 X2 Y2 MS   sanal kaydirma
  dev I 0|1   cihaz durumunu enjekte et (0 masa, 1 yatak, 2 led)
  wait S      saniye bekle
  mem         bellek durumu
Gerekenler: pip install pyserial pillow
"""
import base64, sys, time
import serial
from PIL import Image


def read_until(ser, marker, timeout):
    t0, lines = time.time(), []
    while time.time() - t0 < timeout:
        raw = ser.readline()
        if not raw:
            continue
        line = raw.decode(errors="ignore").strip()
        lines.append(line)
        if line.startswith(marker):
            return lines
    return lines


def decode(lines, w, h):
    px = [0] * (w * h)
    pos = {f"D{i}": i * w * (h // 4) for i in range(4)}
    for line in lines:
        if line[:2] in pos and line[2:3] == " ":
            data = base64.b64decode(line[3:])
            for i in range(0, len(data) - 2, 3):
                cnt, v = data[i], data[i + 1] | (data[i + 2] << 8)
                k = line[:2]
                for _ in range(cnt):
                    px[pos[k]] = v
                    pos[k] += 1
    img = Image.new("RGB", (w, h))
    img.putdata([(((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31) for v in px])
    return img


def main():
    port, prefix, cmds = sys.argv[1], sys.argv[2], sys.argv[3:]
    ser = serial.Serial(port, 115200, timeout=0.5)
    time.sleep(3)                       # port acilinca kart yeniden baslar
    ser.reset_input_buffer()
    n = 0
    for c in cmds:
        if c.startswith("wait"):
            time.sleep(float(c.split()[1])); continue
        ser.write((c + "\n").encode())
        if c == "snap":
            lines = read_until(ser, "SNAPEND", 120)
            hdr = [l for l in lines if l.startswith("SNAPHDR")]
            if not hdr:
                print("snap: basarisiz", lines[-3:]); continue
            _, w, h = hdr[0].split()
            n += 1
            out = f"{prefix}_{n}.png"
            decode(lines, int(w), int(h)).save(out)
            print("kaydedildi", out)
        else:
            time.sleep(0.4)
            while ser.in_waiting:
                print(ser.readline().decode(errors="ignore").strip())
    ser.close()


main()
