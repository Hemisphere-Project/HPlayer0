// Sync hooks — the seam where the Nowde slave role plugs in later (ESP-NOW mesh clock +
// MediaSync packets, see docs/DESIGN.md). Today it is a stub: the player reports what
// it does, and asks whether the sync wants a different cue. Nothing here touches the radio.
#pragma once
#include <Arduino.h>

struct SyncCommand {
  bool valid = false;
  int cueIndex = 0;        // numeric file prefix to play, 0 = stop
  uint32_t positionMs = 0; // where the master is
  bool playing = false;
};

namespace syncgrp {
void begin();
void reportTrack(int cueIndex, const char* name, uint32_t durationMs);
void reportPosition(uint32_t positionMs, bool playing);
bool poll(SyncCommand& out);   // true when a command is pending
const char* stateName();       // "off" until a real backend exists
bool linked();
}
