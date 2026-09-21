#include <Arduino.h>

// Role testi: role IN pinini 2 sn LOW, 2 sn HIGH yapar ve her degisimi seri porta yazar.
// Amac: rolenin hangi seviyede calistigini (cogu modul LOW = calisir) ve kablolarini 220V'a gecmeden dogrulamak.
//   - "GPIO5 = LOW" iken role TIK yapip modul LED'i yaniyorsa modulun ters mantikli (active-low) oldugunu goruyorsun.
//   - HIGH iken tik yapiyorsa modul normal mantikli: hub/yatak yazilimlarinda RELAY_ACTIVE_LOW'u 0 yap.
// Multimetreyle COM/NO/NC uclarini bulmak icin docs/en/hardware.md dosyasina bak.

#define RELAY_PIN 5   // hub ve yatak dugumundeki varsayilan pin

void setup() {
  Serial.begin(115200);
  delay(1500);
  gpio_set_level((gpio_num_t)RELAY_PIN, 1);   // acilista role kapali kalsin (active-low modullerde HIGH = kapali)
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  Serial.printf("\nRole testi basladi. Pin: GPIO%d. Role VCC'yi 5V pinine, GND'yi GND'ye bagla.\n", RELAY_PIN);
}

void loop() {
  digitalWrite(RELAY_PIN, LOW);
  Serial.printf("GPIO%d = LOW   (active-low modulde role CALISIR: tik)\n", RELAY_PIN);
  delay(2000);
  digitalWrite(RELAY_PIN, HIGH);
  Serial.printf("GPIO%d = HIGH  (active-low modulde role BIRAKIR)\n", RELAY_PIN);
  delay(2000);
}
