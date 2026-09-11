// Persistent settings (NVS namespace "hp0"). Writes are coalesced: call markDirty()
// and let tick() flush a couple of seconds after the last change, so a held volume
// button does not hammer the flash.
#pragma once
#include <Arduino.h>

struct Settings {
  uint8_t  volume     = 60;    // 0..100, codec DAC level
  uint8_t  brightness = 160;   // 10..255
  uint16_t dimAfterS  = 300;   // seconds idle before the backlight dims, 0 = never
  uint8_t  rebootEveryH = 0;   // uptime hours before a between-tracks reboot, 0 = off
  uint8_t  ledLevel   = 20;    // module RGB LED brightness 0..100, 0 = leds off
};

namespace settings {
void load(Settings& s);
void markDirty();
void tick(const Settings& s);   // flushes when dirty and the delay elapsed
void flushNow(const Settings& s);
}
