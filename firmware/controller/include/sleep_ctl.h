#pragma once

// Derin uyku: pilde (harici guc yokken) ekran kapaliyken WiFi/ESP-NOW radyosu dahil her sey kapanir.
// V1 kartta dokunmatik kesme pini yok, bu yuzden 300 ms'de bir uyanip dokunmaya bakilir.

void sleep_deep_poll(void);        // ilk uyku girisi: LCD pinlerini yuksek tut, timer ile uyan
void sleep_poll_again(void);       // dokunma yoksa hemen tekrar uyu (pinler zaten tutuluyor)
void sleep_release_holds(void);    // gercek acilista LCD pin tutmalarini birak
