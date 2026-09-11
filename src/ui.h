// Screen: 320x240 through one full-screen sprite (PSRAM), pushed under the SPI bus lock.
// Retro-future palette: black ground, cyan structure, yellow for what plays or is selected,
// green for readouts, red for alerts. VT323 (Latin-1, accents) for text, Orbitron header.
// Track list vertically centred on the playing track: its number, a marquee title when it
// does not fit, position / duration and a progress bar. Tap a row to play it.
#pragma once
#include <Arduino.h>

#include "player.h"

struct UiStatus {
  bool sdMounted = false;
  size_t trackCount = 0;
  const char* syncState = "off";
  bool syncLinked = false;
  bool menuOpen = false;
};

namespace ui {
void begin(uint8_t brightness);
void bootScreen();
void bootLine(const char* fmt, ...);
void render(const PlayerSnapshot& s, const UiStatus& st);
bool animating();                 // a marquee is running: keep rendering at UI_PERIOD_MS
int trackAtY(int y, const PlayerSnapshot& s, size_t count);   // list row under a touch, -1 if none
int menuItemAtY(int y);                                        // menu entry under a touch, -1 if none
}
