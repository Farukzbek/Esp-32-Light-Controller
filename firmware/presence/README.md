# Varlık düğümü (deney): WiFi CSI ile hareket algılama

ESP32-S3 Super Mini, modeme saniyede ~25 ping atar ve modemin cevap paketlerinin CSI'ından (kanal durum bilgisi) hareket skoru üretir.
**Durum: deney aşaması, çalışan bir sürüm değil.** Bulgular:

- Seri komutlar: `wifi <ağ> <şifre>`, `ip <adres>`, `status`. Tarayıcıdan `http://<ip>/` canlı grafik, `/api/csi` JSON, `tools/record.py <ip>` yönergeli kayıt.
- Üç ölçüt yan yana: `old` (anlık sapma, ayırt ediciliği düşük), `win` (1 sn pencere), `corr` (1 - korelasyon; sakinken 0.02-0.03, hareketle 0.1-0.5, en ayırt edici).
- **Sorun:** Bu kartta (ESP32-S3, Arduino-ESP32 3.x) modemin ping cevapları CSI geri çağrımını neredeyse hiç tetiklemiyor (50 sn'de ~84 geri çağrım, ~1,5/sn, 1678 ping cevabına karşı). İlk ölçümlerdeki paketler, sayfanın kendi 200 ms'lik istek trafiğinden geliyordu, bu yüzden o sayılar güvenilir değil.
- Denenmesi gerekenler: iki ESP32 arasında ESP-NOW yayınıyla CSI (Espressif esp-csi bu yolu kullanır), promiscuous mod, protokol/bant genişliği sabitleme.
