#pragma once
#include <stdint.h>
#include <stdbool.h>

// ESP-NOW (HomeSpan SpanPoint) ile masa ESP32 (hub) baglantisi. Ayri bir FreeRTOS gorevinde calisir,
// arayuz (LVGL) hicbir zaman ag islemi icin beklemez.

void now_init(void);
void now_send_set(uint8_t dev, bool on);   // cihazi ac/kapat komutu (kuyruga atar)
void now_query(void);                      // hub'dan guncel durumu iste
bool now_take_state(uint8_t st[4]);        // yeni durum geldiyse true (masa, yatak, led, -)
bool now_take_failed(void);                // bir SET hub'a ulasamadiysa true
bool now_link_ok(void);                    // son 30 sn icinde hub ile temas var mi

// Sessiz cihazlar: bit0 = hub (masa/led), bit1 = yatak
uint8_t now_link_down_mask(void);

// Sinyal gucu (dBm) ve son olcumden beri gecen sure; link 0 = hub, 1 = yatak. Olcum yoksa 0.
int8_t now_rssi(int link, uint32_t *ageMs);

// Sinyal olcumu sadece istenince acilir (promiscuous mod radyoyu/islemciyi yorar): durum sayfasi acikken.
void now_rssi_enable(bool on);
uint8_t now_channel(void);   // radyonun su anki kanali
