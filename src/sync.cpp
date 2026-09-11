#include "sync.h"

namespace syncgrp {
void begin() {}
void reportTrack(int, const char*, uint32_t) {}
void reportPosition(uint32_t, bool) {}
bool poll(SyncCommand& out) {
  out.valid = false;
  return false;
}
const char* stateName() { return "off"; }
bool linked() { return false; }
}  // namespace syncgrp
