#pragma once
#include <stdint.h>

// Kumanda paneli (CYD) <-> masa dugumu (hub) ve yatak dugumu ESP-NOW protokolu.
// Iki uc da BU dosyayi kullanir: mesaj boyutu (sizeof(NowMsg)) iki tarafta ayni olmak zorunda.

// Cihaza ozel ayarlar (MAC adresleri, ESP-NOW kanali, sifreleme parolasi) now_config.h icinde.
#ifndef NOW_NO_CONFIG   // genel dugum (firmware/node): ayar derleme zamaninda degil, eslesirken gelir
#if __has_include("now_config.h")
#include "now_config.h"
#else
#error "now_config.h yok: firmware/shared/now_config.example.h dosyasini now_config.h olarak kopyalayip doldur (bkz. docs/setup.md)"
#endif
#endif

enum : uint8_t { NOW_SET = 1, NOW_QUERY = 2, NOW_STATE = 3, NOW_CFG = 4, NOW_FORGET = 5 };
// NOW_CFG (kopru -> eslesen dugum): dev = yuva, val = role GPIO pini, st[0] bit0 = ters mantik (LOW = acik). NOW_FORGET: dev = yuva, eslesmeyi sil ve yeniden basla.
// --- Servo ve PIR rolleri (eslesmede kopru rolu secer: PairSecret.reserved[0], bkz. pair_proto.h). Hepsi ayni 10 baytlik NowMsg'a sigar.
// NOW_SERVO_CFG   (kopru -> dugum): dev = yuva, val = servo pini, st[0] bit0 = ters yon, st[1] = KAPALI acisi (0..180), st[2] = ACIK acisi, st[3] = hiz (10 derece/sn biriminde, 0 = aninda)
// NOW_SERVO_CFG2  (kopru -> dugum): dev = yuva, val = birakma suresi (100 ms biriminde; 0 = asla birakma, surekli tut), st[0] = en kucuk aci siniri, st[1] = en buyuk aci siniri
// NOW_SERVO_GOTO  (kopru -> dugum): dev = yuva, val = hedef aci (0..180), st[0] = hiz (10 derece/sn; NOW_UNKNOWN = ayardaki hiz)
// NOW_SERVO_FREE  (kopru -> dugum): dev = yuva, servoyu hemen BIRAK (PWM kesilir, elle oynatilabilir)
// NOW_ANGLE       (dugum -> kopru): dev = yuva, val = HEDEF aci (son komut), st[0] bit0 = serbest (birakilmis), bit1 = HAREKET EDIYOR, st[1] = su anki aci, seq = ilgili komutun seq'i
// NOW_PIR_CFG     (kopru -> dugum): dev = yuva, val = PIR pini, st[0] = tutma suresi (sn), st[1] = onay suresi (10 ms biriminde: bu kadar sureden kisa darbeler yok sayilir), st[2] = bekleme (sn, temizlendikten sonra yeniden tetik yok), st[3] bit0 = ters (LOW = hareket)
// PIR dugumu hareketi STATE mesajinin val alaninda bildirir (1 = hareket var, 0 = yok); servo dugumu STATE val = 1 (ACIK acisinda) / 0.
enum : uint8_t { NOW_SERVO_CFG = 6, NOW_SERVO_CFG2 = 7, NOW_SERVO_GOTO = 8, NOW_SERVO_FREE = 9, NOW_ANGLE = 12, NOW_PIR_CFG = 10 };
enum : uint8_t { ROLE_LIGHT = 0, ROLE_SERVO = 1, ROLE_PIR = 2 };
enum : uint8_t { DEV_MASA = 0, DEV_YATAK = 1, DEV_LED = 2, DEV_COUNT = 3 };

// STATE mesajinda st[i] == NOW_UNKNOWN ise gonderen o cihazin sahibi degildir, degeri degistirme.
#define NOW_UNKNOWN 0xFF

struct __attribute__((packed)) NowMsg {
  uint8_t  type;   // NOW_SET / NOW_QUERY / NOW_STATE
  uint8_t  dev;    // SET: hangi cihaz
  uint8_t  val;    // SET: 0 / 1
  uint8_t  st[4];  // STATE: cihaz durumlari (masa, yatak, led, rezerve)
  uint16_t seq;    // istek numarasi; STATE yanitinda ayni numara, hub'in kendi bildirimlerinde 0
};
