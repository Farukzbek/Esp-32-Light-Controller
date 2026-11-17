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
#ifdef NOW_GENERIC
#define NOW_DYN_MAX   5     // genel kopru: sabit hub/yatak yok, 5 eslesen dugum (yuva 3..7)
#else
#define NOW_DYN_MAX   4     // kisisel derleme: hub + yatak + 4 dugum = 6 sifreli esle
#endif
#ifdef __cplusplus
#include <Arduino.h>
/// ESP-NOW ag anahtari: genel kopruda ilk acilista rastgele uretilir (NVS), kisisel derlemede now_config.h'deki parola.
String now_net_key();
#endif
bool now_dyn_add(uint8_t slot, const uint8_t mac[6]);   // yuvaya dugum bagla (SpanPoint olusturur, NVS'e yazar)
bool now_dyn_present(uint8_t slot);
int8_t now_dyn_state(uint8_t slot);                     // 1 acik, 0 kapali, -1 bilinmiyor
bool now_dyn_linked(uint8_t slot);                      // dugum son zamanlarda konustu mu
void now_send_cfg(uint8_t slot, uint8_t pin, bool inv); // role pini / ters mantik (NOW_CFG)
void now_forget(uint8_t slot);                          // dugume eslesmeyi sildir (NOW_FORGET) ve kaydi sil

// --- Servo ve PIR rolleri (bkz. shared/now_proto.h) ---
uint8_t now_role(uint8_t slot);                         // ROLE_LIGHT / ROLE_SERVO / ROLE_PIR
void now_set_role(uint8_t slot, uint8_t role);          // eslesmede kopru rolu belirler (NVS'e yazar)
int16_t now_dyn_angle(uint8_t slot);                    // servonun son bildirilen acisi, -1 bilinmiyor
bool now_dyn_free(uint8_t slot);                        // servo birakilmis mi (elle oynatilabilir)
// pin, ters yon, kapali acisi, acik acisi, hiz (10 derece/sn; 0 = aninda), birakma suresi (100 ms; 0 = asla), alt ve ust aci siniri
void now_send_servo_cfg(uint8_t slot, uint8_t pin, bool inv, uint8_t offAngle, uint8_t onAngle, uint8_t speed, uint8_t releaseDs, uint8_t minAngle, uint8_t maxAngle);
void now_send_servo_goto(uint8_t slot, uint8_t angle, int speed);   // speed < 0: ayardaki hiz
void now_send_servo_free(uint8_t slot);                              // servoyu hemen birak
// PIR: pin, tutma suresi (sn), onay suresi (10 ms), bekleme (sn), ters (LOW = hareket)
void now_send_pir_cfg(uint8_t slot, uint8_t pin, uint8_t holdS, uint8_t confirm10ms, uint8_t cooldownS, bool inv);
