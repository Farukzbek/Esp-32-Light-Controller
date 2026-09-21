#include "battery.h"
#include <Arduino.h>

#define BAT_ADC_PIN 4

float battery_read_vcc(void) {
  uint32_t sum = 0;
  const int n = 16;
  for (int i = 0; i < n; i++) sum += analogReadMilliVolts(BAT_ADC_PIN);
  return (sum / (float)n / 1000.0f) * 3.0f;
}
