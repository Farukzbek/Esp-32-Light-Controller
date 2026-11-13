#pragma once
// Arayuz dili. Varsayilan Turkce (Turkce karakterli yazi tipi src/fonts/); Ingilizce icin: pio run -e cyd-en
// Seri porta yazilan teshis mesajlari her iki dilde Turkce kalir.
#ifndef UI_LANG_EN
#define UI_LANG_EN 0
#endif

#if UI_LANG_EN
  #define T_ITEM_FULL   {"Desk lamp", "Bed lamp", "LED strip", "All lights"}
  #define T_ITEM_SHORT  {"Desk", "Bed", "LED strip", "All lights"}
  #define T_ON          "ON"
  #define T_OFF         "OFF"
  #define T_N_ON        "%d/%d ON"
  #define T_CONNECTED   "Connected"
  #define T_WARN_BOTH   "Desk + Bed unreachable"
  #define T_WARN_HUB    "Desk unreachable"
  #define T_WARN_BED    "Bed unreachable"
  #define T_SETTINGS    "Settings"
  #define T_TABS        {"LAYOUT", "SCREEN", "STATUS"}
  #define T_LAYOUTS     {"4-up", "3-up", "2-up", "1-up"}
  #define T_BRIGHT      "Brightness"
  #define T_AUTO        "Auto brightness"
  #define T_AUTO_SUB    "by light sensor"
  #define T_FLIP        "Rotate 180°"
  #define T_SCREEN_OFF  "Screen off after"
  #define T_OFF_OPTS    {"30 s", "1 min", "5 min", "Never"}
  #define T_ST_DESK     "Desk node"
  #define T_ST_BED      "Bed node"
  #define T_ST_CHANNEL  "ESP-NOW channel"
  #define T_ST_NONE     "OFFLINE"
  #define T_CAL         "Calibrate touch"
  #define T_VERSION     "v1.0"
#else
  #define T_ITEM_FULL   {"Masa lambası", "Yatak lambası", "LED şerit", "Tüm ışıklar"}
  #define T_ITEM_SHORT  {"Masa", "Yatak", "LED şerit", "Tüm ışıklar"}
  #define T_ON          "AÇIK"
  #define T_OFF         "KAPALI"
  #define T_N_ON        "%d/%d AÇIK"
  #define T_CONNECTED   "Bağlı"
  #define T_WARN_BOTH   "Masa + Yatak ulaşılamıyor"
  #define T_WARN_HUB    "Masa ulaşılamıyor"
  #define T_WARN_BED    "Yatak ulaşılamıyor"
  #define T_SETTINGS    "Ayarlar"
  #define T_TABS        {"DÜZEN", "EKRAN", "DURUM"}
  #define T_LAYOUTS     {"4'lü", "3'lü", "2'li", "1'li"}
  #define T_BRIGHT      "Parlaklık"
  #define T_AUTO        "Otomatik parlaklık"
  #define T_AUTO_SUB    "ışık sensörüne göre"
  #define T_FLIP        "Ekranı çevir (180°)"
  #define T_SCREEN_OFF  "Ekranı kapat"
  #define T_OFF_OPTS    {"30 sn", "1 dk", "5 dk", "Hiç"}
  #define T_ST_DESK     "Masa düğümü"
  #define T_ST_BED      "Yatak düğümü"
  #define T_ST_CHANNEL  "ESP-NOW kanalı"
  #define T_ST_NONE     "YOK"
  #define T_CAL         "Dokunmatiği kalibre et"
  #define T_VERSION     "v1.0"
#endif
