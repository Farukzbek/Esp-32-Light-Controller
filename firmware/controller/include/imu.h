#pragma once

// I2C, Touch_Init() tarafindan baslatilmis olmali (ayni hat paylasiliyor)
void imu_init(void);

// Jiroskobu okur. LVGL task'i icinden cagrilmali (I2C hatti paylasiliyor).
void imu_update(void);

// Aci hizi, derece/saniye
float imu_get_gx(void);
float imu_get_gy(void);
float imu_get_gz(void);

// Ivme Z ekseni (g). Ekran yukari bakacak sekilde duz yatarken ~ +1, dik tutunca ~ 0.
float imu_get_az(void);
