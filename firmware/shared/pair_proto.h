#pragma once
#include <stdint.h>
#include <string.h>
#include "x25519.h"

// Cihaz eslestirme (pairing) protokolu. Ham, SIFRELENMEMIS ESP-NOW; yalnizca kopru "eslesme penceresi"ndeyken (uygulamadan acilir, 2 dk).
//
//   kopru  --BEACON (yayin, 300 ms'de bir)-->  eslesmemis dugumler tum kanallari tarar, beacon'u duydugu kanalda kilitlenir
//   dugum  --HELLO  (yayin, ~1 sn'de bir)-->   kopru dinler (promiscuous), adayi listeler; uygulamada kullanici secer
//   kopru  --ACCEPT (dugume)-->                ag parolasi + kanal + yuva, X25519 + AES-256-GCM ile SIFRELI
//
// Anahtar degisimi: dugum ve kopru her eslesmede gecici X25519 anahtar cifti uretir, acik anahtarlar HELLO/ACCEPT'te gider.
// Sifre anahtari = SHA-256(ortak_gizli || dugum_MAC || kopru_MAC || "nowlight-pair-v1"); AAD = dugum_MAC || kopru_MAC.
// Pencere disindan ag parolasi havadan duz metin gecmez. Sinir: eslesme penceresinde araya giren aktif bir saldirgan
// (sahte dugum/kopru) bu yontemle engellenmez; pencereyi kisa tut ve yalnizca kendin baslat.

#define PAIR_MAGIC0 'N'
#define PAIR_MAGIC1 'L'
#define PAIR_VERSION 2
enum : uint8_t { PAIR_BEACON = 1, PAIR_HELLO = 2, PAIR_ACCEPT = 3 };
enum : uint8_t { KIND_RELAY = 1 };       // role ile ac/kapat yapan isik dugumu
enum : uint8_t { CHIP_ESP32 = 1, CHIP_S2 = 2, CHIP_S3 = 3, CHIP_C3 = 4, CHIP_C6 = 5 };

struct __attribute__((packed)) PairBeacon { uint8_t magic[2]; uint8_t type; uint8_t ver; };
struct __attribute__((packed)) PairHello  { uint8_t magic[2]; uint8_t type; uint8_t ver; uint8_t kind; uint8_t chip; uint8_t defPin; uint8_t defInv; char name[16]; uint8_t pub[32]; };
struct __attribute__((packed)) PairSecret { char pass[32]; uint8_t channel; uint8_t slot; uint8_t pin; uint8_t flags; uint8_t reserved[4]; };   // flags bit0 = ters mantik (LOW = acik)
struct __attribute__((packed)) PairAccept { uint8_t magic[2]; uint8_t type; uint8_t ver; uint8_t pub[32]; uint8_t iv[12]; uint8_t tag[16]; uint8_t ct[sizeof(PairSecret)]; };

static inline bool pair_magic_ok(const uint8_t *d) { return d[0] == PAIR_MAGIC0 && d[1] == PAIR_MAGIC1; }

// Yuva numaralari: 0 masa, 1 yatak, 2 led (yerlesik); 3..6 eslesen dugumler (uygulamada "d3".."d6").
#define PAIR_FIRST_SLOT 3
#define PAIR_MAX_NODES  4     // ESP-NOW en fazla 6 sifreli esle: hub + yatak + 4 dugum
