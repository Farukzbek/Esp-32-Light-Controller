#!/bin/bash
# Kopru ve kapi dugumu yazilimlarini web yukleyiciye (web/flasher) KISISEL girdi olarak ekler.
# Bunlar now_config.h (MAC, parola) ile derlenir: herkese acik sayfaya KOYMA (git'e girmez, .gitignore'da).
# Parcalar ayri ofsetlerde yazilir: NVS (WiFi, token, sabit IP, eslesen dugumler) silinmez.
set -e
cd "$(dirname "$0")"
OUT="../web/flasher/firmware"
APPS="$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"

build() {  # dizin ortam hedef_ad baslik yonga bootloader_ofseti
  local dir=$1 env=$2 name=$3 title=$4 chip=$5 boot=$6
  echo "=== $name"
  ( cd "$dir" && pio run -e "$env" >/dev/null )
  mkdir -p "$OUT/$name"
  cp "$dir/.pio/build/$env/bootloader.bin" "$dir/.pio/build/$env/partitions.bin" "$dir/.pio/build/$env/firmware.bin" "$OUT/$name/"
  cp "$APPS" "$OUT/$name/boot_app0.bin"
  cat > "$OUT/$name.json" <<JSON
{
  "name": "$title",
  "version": "$(date +%Y.%m.%d-%H%M)",
  "new_install_prompt_erase": false,
  "builds": [ { "chipFamily": "$chip", "parts": [
    { "path": "$name/bootloader.bin", "offset": $boot },
    { "path": "$name/partitions.bin", "offset": 32768 },
    { "path": "$name/boot_app0.bin", "offset": 57344 },
    { "path": "$name/firmware.bin", "offset": 65536 }
  ] } ]
}
JSON
}

build bridge bridge kopru "Nowlight koprusu (kisisel)" "ESP32" 4096
build door presence kapi "Nowlight kapi dugumu (kisisel)" "ESP32-S3" 0
echo "Tamam: $OUT/{kopru,kapi}.json"
