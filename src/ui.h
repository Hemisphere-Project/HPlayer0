// Screen: 320x240 through one full-screen sprite (PSRAM), pushed under the SPI bus lock.
// Track list vertically centred on the playing track, position / duration and a progress
// bar on that row, status on top, the three button labels at the bottom.
#pragma once
#include <Arduino.h>

#include "player.h"

struct UiStatus {
  bool sdMounted = false;
  size_t trackCount = 0;
  const char* syncState = "off";
  bool menuOpen = false;
};

namespace ui {
void begin(uint8_t brightness);
void bootScreen();
void bootLine(const char* fmt, ...);
void render(const PlayerSnapshot& s, const UiStatus& st);
}
