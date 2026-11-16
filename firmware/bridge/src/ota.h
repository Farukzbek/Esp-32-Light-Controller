#pragma once
#include <Arduino.h>

// Kablosuz (OTA) guncelleme: kopru yeni yazilimi flash.matestudios.online'dan HTTPS ile kendisi indirir (sertifika dogrulamali).
// Yalnizca genel kopru derlemesinde (NL_BRIDGE_KEY tanimli, OTA bolumlu) calisir; kisisel derleme USB ile guncellenir.
bool ota_supported();
String ota_version_json();                 // {"version","chip","ota","generic"}
String ota_status_json();                  // {"state","current","latest","available","progress","error"}
void ota_check();                          // yeni surum var mi? (arka planda)
bool ota_start();                          // son surumu indir ve yaz (arka planda); bitince yeniden baslar
