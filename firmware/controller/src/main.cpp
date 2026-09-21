#include "lcd_bsp.h"
#include "FT3168.h"
#include "imu.h"
#include "now_link.h"
#include "sleep_ctl.h"
#include "esp_sleep.h"

RTC_DATA_ATTR static uint32_t rtc_polls = 0;   // derin uykuda kac kez dokunma kontrolu yapildi

void setup() {
  const bool timerWake = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER);

  if (timerWake) {
    // Periyodik uyanma: sadece dokunmaya bak. Yoksa ekrani/radyoyu hic acmadan hemen tekrar uyu.
    rtc_polls++;
    Touch_Init();
    uint16_t tx, ty;
    if (!getTouch(&tx, &ty)) sleep_poll_again();   // donmez
    // Dokunma var: gercek acilis. Bu dokunusu ekrandaki butona ceviremeyiz, parmak kalkana kadar yut.
    g_lcd_swallow_boot = true;
    sleep_release_holds();
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0);
    Serial.printf("[sleep] dokunmayla uyandi (%lu kontrol sonra)\n", (unsigned long)rtc_polls);
    rtc_polls = 0;
  } else {
    Serial.begin(115200);
    // Terminal bagli degilken Serial yazmasi bloklanmasin (LVGL task'ini durdurur)
    Serial.setTxTimeoutMs(0);
    delay(1500); // USB CDC'nin baglanmasi icin bekle, yoksa ilk mesajlar kayboluyor
  }

  if (!timerWake) Touch_Init();
  imu_init();
  now_init();
  lcd_lvgl_Init(); // ic taraftan app_ui_init() cagirir
}

void loop() {
  delay(1000); // her sey LVGL task'inda donuyor
}
