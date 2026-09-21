#include "lcd_bsp.h"
#include "esp_lcd_sh8601.h"
#include "lcd_config.h"
#include "FT3168.h"
static SemaphoreHandle_t lvgl_mux = NULL; //mutex semaphores
#define LCD_HOST    SPI2_HOST

//#define EXAMPLE_Rotate_90
#define SH8601_ID 0x86
#define CO5300_ID 0xff



static esp_lcd_panel_io_handle_t amoled_panel_io_handle = NULL; 

static const sh8601_lcd_init_cmd_t lcd_init_cmds[] = 
{
  {0x11, (uint8_t []){0x00}, 0, 80},   
  {0xC4, (uint8_t []){0x80}, 1, 0},
  
  {0x35, (uint8_t []){0x00}, 1, 0},

  {0x53, (uint8_t []){0x20}, 1, 1},
  {0x63, (uint8_t []){0xFF}, 1, 1},
  {0x51, (uint8_t []){0x00}, 1, 1},

  {0x29, (uint8_t []){0x00}, 0, 10},

  {0x51, (uint8_t []){0xFF}, 1, 0},    //亮度
};

// Ekran donuklugu: 0 = dikey, 1 = yatay (A), 2 = yatay (B).
// LVGL'in sw_rotate'i bu panelde bozuk goruntu verdigi icin donme flush
// icinde kendi kodumuzla yapiliyor.
volatile int g_lcd_rot = 0;
volatile bool g_lcd_touched = false;
volatile bool g_lcd_screen_off = false; // ekran kapaliyken ilk dokunus sadece uyandirir
volatile bool g_lcd_wake_req = false;
volatile bool g_lcd_swallow_boot = false; // derin uykudan dokunmayla uyanildi: ilk dokunusu yut
volatile uint32_t g_perf_handler_max_us = 0;
volatile uint32_t g_perf_touch_gap_max_us = 0;
volatile uint32_t g_perf_touch_miss = 0;
static lv_disp_t *s_disp = NULL;
static lv_disp_drv_t disp_drv;      // contains callback functions
static lv_color_t *s_rot_buf = NULL;

void lcd_set_rotation(int rot)
{
  g_lcd_rot = rot;
  disp_drv.hor_res = (rot == 0) ? EXAMPLE_LCD_H_RES : EXAMPLE_LCD_V_RES;
  disp_drv.ver_res = (rot == 0) ? EXAMPLE_LCD_V_RES : EXAMPLE_LCD_H_RES;
  lv_disp_drv_update(s_disp, &disp_drv);
}

void lcd_lvgl_Init(void)
{
  static lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)

  const spi_bus_config_t buscfg = SH8601_PANEL_BUS_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_PCLK,
                                                               EXAMPLE_PIN_NUM_LCD_DATA0,
                                                               EXAMPLE_PIN_NUM_LCD_DATA1,
                                                               EXAMPLE_PIN_NUM_LCD_DATA2,
                                                               EXAMPLE_PIN_NUM_LCD_DATA3,
                                                               EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * LCD_BIT_PER_PIXEL / 8);
  printf("[lcd] CS=%d PCLK=%d D0=%d D1=%d D2=%d D3=%d RST=%d\n",
                EXAMPLE_PIN_NUM_LCD_CS, EXAMPLE_PIN_NUM_LCD_PCLK,
                EXAMPLE_PIN_NUM_LCD_DATA0, EXAMPLE_PIN_NUM_LCD_DATA1,
                EXAMPLE_PIN_NUM_LCD_DATA2, EXAMPLE_PIN_NUM_LCD_DATA3,
                EXAMPLE_PIN_NUM_LCD_RST);

  esp_err_t err;
  err = spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
  printf("[lcd] spi_bus_initialize: %s\n", esp_err_to_name(err));
  esp_lcd_panel_io_handle_t io_handle = NULL;

  const esp_lcd_panel_io_spi_config_t io_config = SH8601_PANEL_IO_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_CS,
                                                                              example_notify_lvgl_flush_ready,
                                                                              &disp_drv);

  sh8601_vendor_config_t vendor_config =
  {
    .init_cmds = lcd_init_cmds,
    .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
    .flags =
    {
      .use_qspi_interface = 1,
    },
  };
  err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle);
  printf("[lcd] new_panel_io_spi: %s (handle=%p)\n", esp_err_to_name(err), (void *)io_handle);
  if (err != ESP_OK || io_handle == NULL) {
    printf("[lcd] io handle NULL, durduruluyor (cip cokmesin diye)\n");
    return;
  }
  amoled_panel_io_handle = io_handle;
  esp_lcd_panel_handle_t panel_handle = NULL;
  const esp_lcd_panel_dev_config_t panel_config =
  {
    .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
    .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
    .bits_per_pixel = LCD_BIT_PER_PIXEL,
    .vendor_config = &vendor_config,
  };
  err = esp_lcd_new_panel_sh8601(io_handle, &panel_config, &panel_handle);
  printf("[lcd] new_panel_sh8601: %s (handle=%p)\n", esp_err_to_name(err), (void *)panel_handle);
  if (err != ESP_OK || panel_handle == NULL) {
    printf("[lcd] panel handle NULL, durduruluyor (cip cokmesin diye)\n");
    return;
  }
  err = esp_lcd_panel_reset(panel_handle);
  printf("[lcd] panel_reset: %s\n", esp_err_to_name(err));
  err = esp_lcd_panel_init(panel_handle);
  printf("[lcd] panel_init: %s\n", esp_err_to_name(err));
  err = esp_lcd_panel_disp_on_off(panel_handle, true);
  printf("[lcd] disp_on_off: %s\n", esp_err_to_name(err));

  lv_init();
  lv_color_t *buf1 = heap_caps_malloc(EXAMPLE_LCD_H_RES * EXAMPLE_LVGL_BUF_HEIGHT * sizeof(lv_color_t), MALLOC_CAP_DMA);
  assert(buf1);
  lv_color_t *buf2 = heap_caps_malloc(EXAMPLE_LCD_H_RES * EXAMPLE_LVGL_BUF_HEIGHT * sizeof(lv_color_t), MALLOC_CAP_DMA);
  assert(buf2);
  s_rot_buf = heap_caps_malloc(EXAMPLE_LCD_H_RES * EXAMPLE_LVGL_BUF_HEIGHT * sizeof(lv_color_t), MALLOC_CAP_DMA);
  assert(s_rot_buf);
  lv_disp_draw_buf_init(&disp_buf, buf1, buf2, EXAMPLE_LCD_H_RES * EXAMPLE_LVGL_BUF_HEIGHT);
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = EXAMPLE_LCD_H_RES;
  disp_drv.ver_res = EXAMPLE_LCD_V_RES;
  disp_drv.flush_cb = example_lvgl_flush_cb;
  disp_drv.rounder_cb = example_lvgl_rounder_cb;
  disp_drv.draw_buf = &disp_buf;
  disp_drv.user_data = panel_handle;
#ifdef EXAMPLE_Rotate_90
  disp_drv.sw_rotate = 1;
  disp_drv.rotated = LV_DISP_ROT_270;
#endif
  lv_disp_t *disp = lv_disp_drv_register(&disp_drv);
  s_disp = disp;

  static lv_indev_drv_t indev_drv;    // Input device driver (Touch)
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.disp = disp;
  indev_drv.read_cb = example_lvgl_touch_cb;
  lv_indev_drv_register(&indev_drv);

  const esp_timer_create_args_t lvgl_tick_timer_args = 
  {
    .callback = &example_increase_lvgl_tick,
    .name = "lvgl_tick"
  };
  esp_timer_handle_t lvgl_tick_timer = NULL;
  ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

  lvgl_mux = xSemaphoreCreateMutex(); //mutex semaphores
  assert(lvgl_mux);
  xTaskCreate(example_lvgl_port_task, "LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE, NULL, EXAMPLE_LVGL_TASK_PRIORITY, NULL);
  if (example_lvgl_lock(-1))
  {
    app_ui_init();      /* Uygulama arayuzu: src/app_ui.cpp */

    // Release the mutex
    example_lvgl_unlock();
  }
}

static bool example_lvgl_lock(int timeout_ms)
{
  assert(lvgl_mux && "bsp_display_start must be called first");

  const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
  return xSemaphoreTake(lvgl_mux, timeout_ticks) == pdTRUE;
}

static void example_lvgl_unlock(void)
{
  assert(lvgl_mux && "bsp_display_start must be called first");
  xSemaphoreGive(lvgl_mux);
}
static void example_lvgl_port_task(void *arg)
{
  uint32_t task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
  for(;;)
  {
    if (example_lvgl_lock(-1))
    {
      int64_t t0 = esp_timer_get_time();
      task_delay_ms = lv_timer_handler();
      uint32_t dur = (uint32_t)(esp_timer_get_time() - t0);
      if (dur > g_perf_handler_max_us) g_perf_handler_max_us = dur;
      
      example_lvgl_unlock();
    }
    if (task_delay_ms > EXAMPLE_LVGL_TASK_MAX_DELAY_MS)
    {
      task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
    }
    else if (task_delay_ms < EXAMPLE_LVGL_TASK_MIN_DELAY_MS)
    {
      task_delay_ms = EXAMPLE_LVGL_TASK_MIN_DELAY_MS;
    }
    vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
  }
}
static void example_increase_lvgl_tick(void *arg)
{
  lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}
static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
  lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
  lv_disp_flush_ready(disp_driver);
  return false;
}
static void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
  esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
  const int rot = g_lcd_rot;

  if (rot == 0)
  {
    esp_lcd_panel_draw_bitmap(panel_handle, area->x1 + 0x14, area->y1, area->x2 + 0x14 + 1, area->y2 + 1, color_map);
    return;
  }

  // Yatay: alani panelin dikey koordinatlarina dondurup gonder.
  // Rounder alani cift baslangic / tek bitis yaptigi icin donmus alan da hizali kalir.
  const int w = area->x2 - area->x1 + 1;
  const int h = area->y2 - area->y1 + 1;
  const uint16_t *src = (const uint16_t *)color_map;
  uint16_t *dst = (uint16_t *)s_rot_buf;
  int px1, py1;

  if (rot == 1)
  {
    // panel_x = (H_RES-1) - y, panel_y = x
    px1 = (EXAMPLE_LCD_H_RES - 1) - area->y2;
    py1 = area->x1;
    for (int sy = 0; sy < h; sy++)
      for (int sx = 0; sx < w; sx++)
        dst[sx * h + (h - 1 - sy)] = src[sy * w + sx];
  }
  else
  {
    // panel_x = y, panel_y = (V_RES-1) - x
    px1 = area->y1;
    py1 = (EXAMPLE_LCD_V_RES - 1) - area->x2;
    for (int sy = 0; sy < h; sy++)
      for (int sx = 0; sx < w; sx++)
        dst[(w - 1 - sx) * h + sy] = src[sy * w + sx];
  }
  esp_lcd_panel_draw_bitmap(panel_handle, px1 + 0x14, py1, px1 + h + 0x14, py1 + w, s_rot_buf);
}
void example_lvgl_rounder_cb(struct _lv_disp_drv_t *disp_drv, lv_area_t *area)
{
  uint16_t x1 = area->x1;
  uint16_t x2 = area->x2;

  uint16_t y1 = area->y1;
  uint16_t y2 = area->y2;

  // round the start of coordinate down to the nearest 2M number
  area->x1 = (x1 >> 1) << 1;
  area->y1 = (y1 >> 1) << 1;
  // round the end of coordinate up to the nearest 2N+1 number
  area->x2 = ((x2 >> 1) << 1) + 1;
  area->y2 = ((y2 >> 1) << 1) + 1;
}
static void example_lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
  // FT3168 arada tek tuk bos/hatali okuma donebiliyor. Her bos okumayi "parmak kalkti"
  // sayarsak kaydirma ve tiklama iptal olur; bu yuzden en fazla 2 ardisik bos okumayi
  // yok sayip son noktada basili tutuyoruz.
  static int miss = 0;
  static int last_x = 0, last_y = 0;
  static bool swallow = false; // uyandiran dokunusu parmak kalkana kadar yut

  uint16_t tp_x,tp_y;
  uint8_t win = getTouch(&tp_x,&tp_y);
  if (g_lcd_swallow_boot) { g_lcd_swallow_boot = false; if (win) swallow = true; }
  if(win)
  {
    if (g_lcd_screen_off) { g_lcd_wake_req = true; swallow = true; }
    if (swallow)
    {
      data->state = LV_INDEV_STATE_RELEASED;
      return;
    }
    static int64_t last_us = 0;
    int64_t now_us = esp_timer_get_time();
    if (last_us && (now_us - last_us) < 500000 && (uint32_t)(now_us - last_us) > g_perf_touch_gap_max_us)
      g_perf_touch_gap_max_us = (uint32_t)(now_us - last_us);
    last_us = now_us;
    int rot = g_lcd_rot;
    int lx = tp_x, ly = tp_y;
    if (rot == 1) { lx = tp_y; ly = (EXAMPLE_LCD_H_RES - 1) - (int)tp_x; }
    else if (rot == 2) { lx = (EXAMPLE_LCD_V_RES - 1) - (int)tp_y; ly = tp_x; }
    if (lx < 0) lx = 0;
    if (ly < 0) ly = 0;
    last_x = lx;
    last_y = ly;
    miss = 0;
    data->point.x = lx;
    data->point.y = ly;
    data->state = LV_INDEV_STATE_PRESSED;
    g_lcd_touched = true;
  }
  else if (g_lcd_touched && ++miss <= 2)
  {
    g_perf_touch_miss++;
    data->point.x = last_x;
    data->point.y = last_y;
    data->state = LV_INDEV_STATE_PRESSED;
  }
  else
  {
    miss = 0;
    swallow = false;
    data->state = LV_INDEV_STATE_RELEASED;
    g_lcd_touched = false;
  }
}


esp_err_t set_amoled_backlight(uint8_t brig)
{
  uint32_t lcd_cmd = 0x51;
  lcd_cmd &= 0xff;
  lcd_cmd <<= 8;
  lcd_cmd |= 0x02 << 24;
  return esp_lcd_panel_io_tx_param(amoled_panel_io_handle, lcd_cmd, &brig,1);
}