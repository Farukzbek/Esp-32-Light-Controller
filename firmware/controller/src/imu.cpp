#include "imu.h"
#include "driver/i2c.h"
#include <Arduino.h>

#define QMI8658_ADDR   0x6B
#define REG_CTRL1      0x02
#define REG_CTRL2      0x03
#define REG_CTRL3      0x04
#define REG_CTRL7      0x08
#define REG_RESET      0x60
#define REG_AX_L       0x35
#define I2C_PORT       I2C_NUM_0

// Ivmeolcer bu kartta arizali (Y ekseni doygun) - sadece jiroskop kullaniyoruz.
// +-1024 dps, 125 Hz
#define GYRO_SCALE_DPS (1024.0f / 32768.0f)
#define ACC_SCALE_G    (2.0f / 32768.0f)   // +-2g. Ivmeolcerin sadece Z ekseni guvenilir (X ofsetli, Y arizali).

static bool reg_write(uint8_t reg, uint8_t val) {
  uint8_t buf[2] = {reg, val};
  return i2c_master_write_to_device(I2C_PORT, QMI8658_ADDR, buf, 2, 100 / portTICK_PERIOD_MS) == ESP_OK;
}

static bool reg_read(uint8_t reg, uint8_t *buf, size_t len) {
  return i2c_master_write_read_device(I2C_PORT, QMI8658_ADDR, &reg, 1, buf, len, 100 / portTICK_PERIOD_MS) == ESP_OK;
}

static bool s_ok = false;
static float s_gx = 0, s_gy = 0, s_gz = 0, s_az = 0;

void imu_init(void) {
  reg_write(REG_RESET, 0xB0);
  delay(20);
  reg_write(REG_CTRL1, 0x60); // auto-increment + little endian
  reg_write(REG_CTRL2, 0x06); // ivmeolcer: +-2g, 125 Hz (sadece az kullaniliyor)
  reg_write(REG_CTRL3, 0x66); // jiroskop: 1024 dps, 125 Hz
  reg_write(REG_CTRL7, 0x03); // ivmeolcer + jiroskop etkin
  delay(120);                 // jiroskobun ilk gecerli olcumu icin
  s_ok = true;
  imu_update();
  imu_update();
}

void imu_update(void) {
  if (!s_ok) return;
  uint8_t b[12];
  if (!reg_read(REG_AX_L, b, 12)) return;
  s_az = (int16_t)(b[4] | (b[5] << 8)) * ACC_SCALE_G;
  s_gx = (int16_t)(b[6] | (b[7] << 8)) * GYRO_SCALE_DPS;
  s_gy = (int16_t)(b[8] | (b[9] << 8)) * GYRO_SCALE_DPS;
  s_gz = (int16_t)(b[10] | (b[11] << 8)) * GYRO_SCALE_DPS;
}

float imu_get_gx(void) { return s_gx; }
float imu_get_gy(void) { return s_gy; }
float imu_get_gz(void) { return s_gz; }
float imu_get_az(void) { return s_az; }
