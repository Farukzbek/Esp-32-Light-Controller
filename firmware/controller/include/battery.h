#pragma once

// GPIO4 (BAT_ADC), sistem hatti VCC'yi 200K/100K ile boler (1:3).
// USB/dock takiliyken VCC ~ 5V - diyot dusumu, sadece pildeyken ~ pil gerilimi (<= 4.2 V).
float battery_read_vcc(void);
