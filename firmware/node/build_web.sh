#!/bin/bash
# Genel dugum yazilimini derleyip web yukleyicinin (web/flasher) klasorune koyar.
# Kullanim: cd firmware/node && ./build_web.sh
set -e
cd "$(dirname "$0")"
VERSION="0.2.2"   # protokol (pair_proto.h PAIR_VERSION) degisince YENIDEN URET ve YAYINLA: eski dugum yeni kopruyu yok sayar
OUT="../../web/flasher"
mkdir -p "$OUT/firmware"

# ortam | web yukleyicideki yonetici ad | ESP Web Tools yonga ailesi
BOARDS=(
  "esp32s3-supermini|ESP32-S3 Super Mini|ESP32-S3"
  "esp32dev|ESP32 DevKit (klasik)|ESP32"
  "esp32c6|ESP32-C6 DevKit|ESP32-C6"
  "esp32c3|ESP32-C3 (Super Mini, DevKitM, XIAO)|ESP32-C3"
  "esp32s2|ESP32-S2 (Wemos S2 Mini, Saola)|ESP32-S2"
)
for entry in "${BOARDS[@]}"; do
  IFS='|' read -r env title chip <<< "$entry"
  echo "=== $env"
  pio run -e "$env" >/dev/null
  cp ".pio/build/$env/firmware.factory.bin" "$OUT/firmware/$env.bin"
  cat > "$OUT/firmware/$env.json" <<JSON
{
  "name": "Nowlight dugumu ($title)",
  "version": "$VERSION",
  "new_install_prompt_erase": true,
  "builds": [ { "chipFamily": "$chip", "parts": [ { "path": "$env.bin?v=$VERSION", "offset": 0 } ] } ]
}
JSON
done
echo "Tamam: $OUT/firmware"
