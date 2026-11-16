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

// --- Eslesen dugumler (genel isik dugumleri): yuva 3..6 (uygulamada "d3".."d6") ---
#define NOW_DYN_FIRST 3
#define NOW_DYN_MAX   4
bool now_dyn_add(uint8_t slot, const uint8_t mac[6]);   // yuvaya dugum bagla (SpanPoint olusturur, NVS'e yazar)
bool now_dyn_present(uint8_t slot);
int8_t now_dyn_state(uint8_t slot);                     // 1 acik, 0 kapali, -1 bilinmiyor
bool now_dyn_linked(uint8_t slot);                      // dugum son zamanlarda konustu mu
void now_send_cfg(uint8_t slot, uint8_t pin, bool inv); // role pini / ters mantik (NOW_CFG)
void now_forget(uint8_t slot);                          // dugume eslesmeyi sildir (NOW_FORGET) ve kaydi sil
