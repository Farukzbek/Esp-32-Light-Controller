#pragma once

void ui_init(void);                       // ana ekrani kurar, zamanlayicilari baslatir (hal_init()'ten sonra)

#ifdef UI_DEBUG
void ui_debug_set(int dev, bool on);      // gelistirme: seriden cihaz durumu enjekte et
void debug_poll(void);                    // gelistirme: seri komutlar (tap / snap / dev / mem)
#endif
