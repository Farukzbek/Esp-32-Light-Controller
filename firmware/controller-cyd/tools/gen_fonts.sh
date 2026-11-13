#!/bin/sh
# Turkce karakterli LVGL yazi tiplerini uretir (Node.js gerekir; lv_font_conv indirilir).
# Kaynak yazi tipi LVGL paketindeki Montserrat-Medium.ttf (SIL OFL). Calistir: sh tools/gen_fonts.sh
set -e
cd "$(dirname "$0")/.."
TTF=$(find .pio/libdeps -name Montserrat-Medium.ttf | head -1)
[ -n "$TTF" ] || { echo "once 'pio run' ile LVGL'i indir"; exit 1; }
R="0x20-0x7E,0xB0,0xB7,0xC7,0xD6,0xDC,0xE7,0xF6,0xFC,0x11E,0x11F,0x130,0x131,0x15E,0x15F,0x2026"
for sz in 12 14 16 20 24; do
  npx --yes lv_font_conv@1.5.3 --font "$TTF" -r "$R" --size $sz --bpp 4 --format lvgl --lv-include lvgl.h \
    --no-compress -o src/fonts/lv_font_tr_$sz.c --lv-font-name lv_font_tr_$sz
done
