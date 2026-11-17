#pragma once
#include <stdint.h>
#include <math.h>

// Servo ve PIR rollerinin PLATFORM BAGIMSIZ mantigi (donanim/ESP-NOW yok). node/src/main.cpp kullanir, tools/test_role_logic.c dener.

// ----------------------------------------------------------------------------- PIR
// Yazilim filtresi: onay suresinden kisa darbeler yok sayilir; hareket bittikten sonra "tutma" suresi kadar hareket var kalir;
// temizlenince "bekleme" suresi boyunca yeniden tetiklenmez. Algilama ACISI/MENZILI cihazin lensi ve potansiyometresidir (yazilimdan degismez).
struct PirCfg { uint8_t holdS = 5, confirm10 = 5, cooldownS = 0; bool inv = false; };
struct PirState { bool motion = false; uint32_t highSince = 0, lastHigh = 0, clearedAt = 0; };

// pinHigh: pinin ham seviyesi. Donus: hareket durumu DEGISTI mi (true ise kopruye bildir).
inline bool pir_update(const PirCfg &c, PirState &s, bool pinHigh, uint32_t now) {
  const bool raw = pinHigh != c.inv;
  if (raw) {
    if (!s.highSince) s.highSince = now ? now : 1;
    s.lastHigh = now;
    if (!s.motion && now - s.highSince >= (uint32_t)c.confirm10 * 10 &&
        (s.clearedAt == 0 || now - s.clearedAt >= (uint32_t)c.cooldownS * 1000)) { s.motion = true; return true; }
  } else {
    s.highSince = 0;
    if (s.motion && now - s.lastHigh >= (uint32_t)c.holdS * 1000) { s.motion = false; s.clearedAt = now ? now : 1; return true; }
  }
  return false;
}

// ----------------------------------------------------------------------------- SERVO
// Bir adim ilerle: hiz derece/sn, dt sn. Hedefi ASMAZ. hiz <= 0: aninda.
inline float servo_step(float cur, float target, float degPerSec, float dtSec) {
  if (degPerSec <= 0) return target;
  const float step = degPerSec * dtSec, d = target - cur;
  if (fabsf(d) <= step) return target;
  return cur + (d > 0 ? step : -step);
}

inline int servo_clamp(int angle, int minA, int maxA) { return angle < minA ? minA : angle > maxA ? maxA : angle; }

// Servo darbe genisligi (us): 544..2400 us = 0..180 derece; ters yon aciyi 180'den cikarir.
inline uint32_t servo_pulse_us(float logicalAngle, bool inv) {
  const float phys = inv ? 180.0f - logicalAngle : logicalAngle;
  return 544 + (uint32_t)((2400 - 544) * phys / 180.0f);
}

// Hedefe varilali birakma suresi dolduysa birakilmali mi? releaseDs: 100 ms biriminde, 0 = asla birakma.
inline bool servo_should_release(uint8_t releaseDs, uint32_t now, uint32_t doneAt) {
  return releaseDs > 0 && now - doneAt >= (uint32_t)releaseDs * 100;
}
