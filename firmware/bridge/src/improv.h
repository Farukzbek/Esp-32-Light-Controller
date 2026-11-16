#pragma once
#include <Arduino.h>

// Improv Serial (https://www.improv-wifi.com): ESP Web Tools yukleme penceresinden WiFi bilgisi girmeyi saglar,
// seri terminale "wifi ag sifre" yazmaya gerek kalmaz. Paket: "IMPROV" | surum(1) | tur | uzunluk | veri | toplam-kontrol.

/// WiFi bilgisiyle baglanmayi dener; baglanirsa bilgiyi kaydeder ve true doner. Basarisizsa eski ayari geri yukler.
typedef bool (*ImprovConnectFn)(const String &ssid, const String &pass);

void improv_begin(const char *firmwareName, const char *version, const char *chip, const char *deviceName, ImprovConnectFn connect);
/// Seriden gelen her bayti ver; true donerse bayt Improv paketine aittir (metin komutu olarak isleme).
bool improv_feed(uint8_t b);
/// Baglanti durumu degisince (acilista ve sonra) bildir. url: "Cihaza git" baglantisi.
void improv_set_provisioned(bool provisioned, const String &url);
