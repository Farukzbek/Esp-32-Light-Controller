#include <Arduino.h>
#include <Wire.h>

// Waveshare ESP32-S3-Touch-AMOLED-1.64 icin kart kontrolu.
// V1 kartta dokunmatik (FT3168, 0x38) ve IMU (QMI8658, 0x6B) I2C SDA=47 / SCL=48 uzerindedir.
// Bu program o hatti tarar ve "kartin V1 pinout'una uydugunu" soyler.
// Sonucu seri monitorden oku:  pio device monitor -b 115200

#define SDA_PIN 47
#define SCL_PIN 48

void setup() {
  Serial.begin(115200);
  delay(2500);   // USB CDC baglansin
  Serial.println("\n=== Waveshare AMOLED 1.64 kart kontrolu ===");
  Serial.printf("I2C taraniyor: SDA=%d SCL=%d\n", SDA_PIN, SCL_PIN);

  Wire.begin(SDA_PIN, SCL_PIN, 100000);
  bool touch = false, imu = false;
  int found = 0;
  for (uint8_t a = 0x08; a < 0x78; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      found++;
      const char *what = (a == 0x38) ? "  <- FT3168 dokunmatik" : (a == 0x6B) ? "  <- QMI8658 IMU" : "";
      Serial.printf("  cihaz bulundu: 0x%02X%s\n", a, what);
      if (a == 0x38) touch = true;
      if (a == 0x6B) imu = true;
    }
  }
  Serial.printf("Toplam %d cihaz.\n\n", found);

  if (touch && imu) {
    Serial.println("SONUC: OK. Kartin V1 pinout'una uyuyor (dokunmatik + IMU bulundu). Bu depodaki kumanda yazilimi seninle uyumlu.");
  } else if (touch) {
    Serial.println("SONUC: Dokunmatik bulundu ama IMU (0x6B) yok. Kart buyuk ihtimalle V1, IMU adresi/durumu farkli olabilir. Kumanda calisir, otomatik donme calismayabilir.");
  } else {
    Serial.println("SONUC: Dokunmatik (0x38) SDA=47/SCL=48 uzerinde bulunamadi.");
    Serial.println("  Kartin V2 olabilir (pinler farkli): docs/waveshare-amoled-notes.md dosyasina ve Waveshare'in V2 ornek koduna bak,");
    Serial.println("  firmware/controller/include/lcd_config.h icindeki pinleri V2'ye gore degistirmen gerekir.");
  }
}

void loop() { delay(1000); }
