#include <Arduino.h>
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "lcd_config.h"
#include "sleep_ctl.h"

#define WAKE_POLL_US (300 * 1000ULL)

static void holdPinHigh(gpio_num_t pin) {
  gpio_reset_pin(pin);                    // SPI matrisinden cikar, duz GPIO yap
  gpio_set_direction(pin, GPIO_MODE_OUTPUT);
  gpio_set_level(pin, 1);
  gpio_hold_en(pin);
}

void sleep_deep_poll(void) {
  // Yuzen CS/RST uyku boyunca paneli bozabilir: ikisini yuksek tut
  holdPinHigh((gpio_num_t)EXAMPLE_PIN_NUM_LCD_CS);
  holdPinHigh((gpio_num_t)EXAMPLE_PIN_NUM_LCD_RST);
  sleep_poll_again();
}

void sleep_poll_again(void) {
  gpio_deep_sleep_hold_en();
  esp_sleep_enable_timer_wakeup(WAKE_POLL_US);
  esp_deep_sleep_start();                 // donmez
}

void sleep_release_holds(void) {
  gpio_deep_sleep_hold_dis();
  gpio_hold_dis((gpio_num_t)EXAMPLE_PIN_NUM_LCD_CS);
  gpio_hold_dis((gpio_num_t)EXAMPLE_PIN_NUM_LCD_RST);
}
