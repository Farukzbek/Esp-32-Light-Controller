#!/bin/bash
# Herkese acik web yukleyiciyi (yalnizca genel kartlar) Cloudflare Pages'e yayinlar: flash.matestudios.online
set -e
cd "$(dirname "$0")/.."
rm -rf dist/flasher-public && mkdir -p dist/flasher-public/firmware
cp web/flasher/index.html dist/flasher-public/
cp web/flasher/firmware/esp32*.json web/flasher/firmware/esp32*.bin dist/flasher-public/firmware/
cp web/flasher/firmware/bridge-*.json dist/flasher-public/firmware/
cp -R web/flasher/firmware/bridge-* dist/flasher-public/firmware/ 2>/dev/null || true
printf '/firmware/*\n  Cache-Control: no-cache\n' > dist/flasher-public/_headers
cd dist && npx --yes wrangler@3 pages deploy flasher-public --project-name nowlight-flasher --branch main --commit-dirty=true
