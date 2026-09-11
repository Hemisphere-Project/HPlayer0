// The playback engine: ESP32-audioI2S driven from a dedicated pump task (core 0), the
// library's decode task on core 1, and a small state machine polled from the UI task.
// Loops the library in order and wraps. Every failure path ends in "try the next one",
// and the supervisor reboots if even that stops making progress.
#pragma once
#include <Arduino.h>

#include "codec.h"

enum class PlayerState : uint8_t { NoSd, Empty, Starting, Playing, Failed };

struct PlayerSnapshot {
  PlayerState state = PlayerState::NoSd;
  int track = -1;            // library index
  uint32_t posSec = 0;
  uint32_t durSec = 0;
  uint8_t volume = 0;
  uint32_t generation = 0;   // bumps when anything but the position changed
  char codec[8] = "";
};

namespace player {
void begin(const AudioPins& pins, uint8_t volumePct);
void onLibraryChanged();    // after a (re)scan: restart from the first track
void play(int track);       // explicit jump (menu / sync)
void next();
void prev();
void stop();
void setVolume(uint8_t pct);
void tick();                // UI-task supervisor step
PlayerSnapshot snapshot();
const char* stateName(PlayerState s);
bool atTrackBoundary();     // true once, right after a track ended (daily reboot hook)
TaskHandle_t pumpTask();
}

// SD (pump task) and the display (UI task) share one SPI bus: hold this around pushSprite.
SemaphoreHandle_t player_busLock();
