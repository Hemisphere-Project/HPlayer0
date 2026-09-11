// The media library: a read-only scan of the microSD root. Fixed-size table, sorted by
// name (case-insensitive), so playback order is what a file browser shows. Files whose
// name starts with a number carry that number as their cue index (`07_intro.mp3` -> 7),
// the same convention HPlayer2 uses for Nowde media indexes.
#pragma once
#include <Arduino.h>

#include "config.h"

struct Track {
  char name[cfg::MAX_NAME];   // file name with extension, as on the card
  uint32_t size = 0;
  int16_t index = -1;         // numeric prefix, -1 if none
};

namespace library {
bool mount();               // SD.begin, false when no card
void unmount();
bool mounted();
bool cardAlive();           // cheap probe used by the supervisor
size_t scan();              // rebuild the table, returns the count
size_t count();
const Track& at(size_t i);
int findByIndex(int cueIndex);      // first track carrying that numeric prefix, -1 if none
bool supported(const char* name);
uint32_t generation();      // bumps on every scan / unmount
}
