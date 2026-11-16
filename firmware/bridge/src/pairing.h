#pragma once
#include <Arduino.h>
#include "esp_wifi_types.h"

// Eslesme modu (bkz. shared/pair_proto.h). Uygulamadan acilir; pencere 120 sn sonra kendiliginden kapanir.
void pair_start();                                   // pencereyi ac, adaylari temizle
void pair_stop();
bool pair_open();
String pair_json();                                  // {"open":..,"secondsLeft":..,"found":[{"mac","name","rssi"}]}
bool pair_accept(const String &mac, uint8_t slot, String &err);   // adayi yuvaya bagla (parolayi sifreli yollar)
void pair_loop();                                    // ana donguden: beacon yayini ve pencere zamanlayicisi
void pair_sniff(const wifi_promiscuous_pkt_t *p);    // promiscuous geri cagrimindan: HELLO yakala
