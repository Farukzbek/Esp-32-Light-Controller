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

# ---------------------------------------------------------------- GENEL KOPRU (WiFi Improv ile tarayicidan kurulur)
# Parcalar ayri ofsetlerde yazilir (factory.bin degil): NVS (WiFi, token, esli dugumler) guncellemede silinmez.
BRIDGE_VERSION="0.4.0"
APPS="$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
# ortam | anahtar | baslik | yonga ailesi | bootloader ofseti (ESP32 ve S2: 0x1000, digerleri 0x0)
BRIDGES=(
  "bridge-esp32|esp32|ESP32 DevKit (klasik)|ESP32|4096"
  "bridge-s3|s3|ESP32-S3|ESP32-S3|0"
  "bridge-c3|c3|ESP32-C3|ESP32-C3|0"
  "bridge-c6|c6|ESP32-C6|ESP32-C6|0"
  "bridge-s2|s2|ESP32-S2|ESP32-S2|4096"
)
for entry in "${BRIDGES[@]}"; do
  IFS='|' read -r env key title chip boot <<< "$entry"
  echo "=== $env"
  ( cd ../bridge && pio run -e "$env" >/dev/null )
  d="$OUT/firmware/bridge-$key"; mkdir -p "$d"
  cp "../bridge/.pio/build/$env/bootloader.bin" "../bridge/.pio/build/$env/partitions.bin" "../bridge/.pio/build/$env/firmware.bin" "$d/"
  cp "$APPS" "$d/boot_app0.bin"
  cat > "$OUT/firmware/bridge-$key.json" <<JSON
{
  "name": "Nowlight koprusu ($title)",
  "version": "$BRIDGE_VERSION",
  "new_install_prompt_erase": false,
  "builds": [ { "chipFamily": "$chip", "parts": [
    { "path": "bridge-$key/bootloader.bin?v=$BRIDGE_VERSION", "offset": $boot },
    { "path": "bridge-$key/partitions.bin?v=$BRIDGE_VERSION", "offset": 32768 },
    { "path": "bridge-$key/boot_app0.bin?v=$BRIDGE_VERSION", "offset": 57344 },
    { "path": "bridge-$key/firmware.bin?v=$BRIDGE_VERSION", "offset": 65536 }
  ] } ]
}
JSON
done
echo "Kopru paketleri hazir"
