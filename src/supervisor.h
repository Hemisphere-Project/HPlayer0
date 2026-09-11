// Long-run guardian: SD hot-plug, idle backlight dim, module LEDs, scheduled reboot,
// health log, task watchdog. Everything here is polled from the UI task.
#pragma once
#include <Arduino.h>

#include "settings.h"

namespace supervisor {
void begin(Settings* s);
void tick(bool menuOpen);
bool noteInput();   // call on any button press; true when the press only woke the backlight
}
