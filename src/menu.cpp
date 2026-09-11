#include "menu.h"

#include <M5Unified.h>

#include "codec.h"
#include "config.h"
#include "library.h"
#include "sync.h"

namespace {
Settings* g_s = nullptr;
bool g_open = false;
size_t g_sel = 0;

enum Item : size_t {
  BRIGHTNESS = 0, DIM, LED, REBOOT_EVERY,
  INFO_VERSION, INFO_UPTIME, INFO_HEAP, INFO_TRACKS, INFO_SYNC,
  REBOOT_NOW, EXIT, COUNT
};

const uint16_t kDimSteps[] = {0, 60, 120, 300, 600, 1800, 3600};
const uint8_t kRebootSteps[] = {0, 6, 12, 24, 48};

template <typename T, size_t N>
size_t stepIndex(const T (&arr)[N], T v) {
  for (size_t i = 0; i < N; ++i)
    if (arr[i] == v) return i;
  return 0;
}

void apply() {
  M5.Display.setBrightness(g_s->brightness);
  codec::setLedBrightness(g_s->ledLevel);
  settings::markDirty();
}

void adjust(int dir) {
  switch (g_sel) {
    case BRIGHTNESS: {
      int v = g_s->brightness + dir * 15;
      g_s->brightness = v < cfg::BRIGHT_MIN ? cfg::BRIGHT_MIN : (v > 255 ? 255 : v);
      apply();
      break;
    }
    case DIM: {
      size_t i = stepIndex(kDimSteps, g_s->dimAfterS);
      size_t n = sizeof(kDimSteps) / sizeof(kDimSteps[0]);
      i = (i + n + dir) % n;
      g_s->dimAfterS = kDimSteps[i];
      apply();
      break;
    }
    case LED: {
      int v = g_s->ledLevel + dir * 10;
      g_s->ledLevel = v < 0 ? 0 : (v > 100 ? 100 : v);
      apply();
      break;
    }
    case REBOOT_EVERY: {
      size_t i = stepIndex(kRebootSteps, g_s->rebootEveryH);
      size_t n = sizeof(kRebootSteps) / sizeof(kRebootSteps[0]);
      i = (i + n + dir) % n;
      g_s->rebootEveryH = kRebootSteps[i];
      apply();
      break;
    }
    case REBOOT_NOW:
      settings::flushNow(*g_s);
      log_w("reboot requested from menu");
      delay(50);
      ESP.restart();
      break;
    case EXIT:
      g_open = false;
      break;
    default:
      break;
  }
}
}  // namespace

namespace menu {

void begin(Settings* s) { g_s = s; }
void open() { g_open = true; g_sel = 0; }
void close() { g_open = false; }
bool isOpen() { return g_open; }
void onLeft() { adjust(-1); }
void onRight() { adjust(+1); }
void onCenter() { g_sel = (g_sel + 1) % COUNT; }
void select(size_t i) { if (i < COUNT) g_sel = i; }
void activate() { adjust(+1); }
size_t count() { return COUNT; }
size_t selected() { return g_sel; }

bool editable(size_t i) {
  return i == BRIGHTNESS || i == DIM || i == LED || i == REBOOT_EVERY || i == REBOOT_NOW || i == EXIT;
}

void label(size_t i, char* out, size_t n) {
  static const char* const names[COUNT] = {
      "Brightness", "Dim after", "Module LEDs", "Auto reboot",
      "Version", "Uptime", "Heap free", "Tracks", "Sync",
      "Reboot now", "Exit"};
  strlcpy(out, i < COUNT ? names[i] : "?", n);
}

void value(size_t i, char* out, size_t n) {
  switch (i) {
    case BRIGHTNESS: snprintf(out, n, "%d%%", (g_s->brightness * 100) / 255); break;
    case DIM:
      if (g_s->dimAfterS == 0) strlcpy(out, "never", n);
      else if (g_s->dimAfterS < 60) snprintf(out, n, "%us", g_s->dimAfterS);
      else snprintf(out, n, "%u min", g_s->dimAfterS / 60);
      break;
    case LED: g_s->ledLevel ? snprintf(out, n, "%u%%", g_s->ledLevel) : strlcpy(out, "off", n); break;
    case REBOOT_EVERY: g_s->rebootEveryH ? snprintf(out, n, "every %uh", g_s->rebootEveryH) : strlcpy(out, "off", n); break;
    case INFO_VERSION: snprintf(out, n, "%s %s", HP_VERSION, HP_BOARD); break;
    case INFO_UPTIME: {
      uint32_t s = millis() / 1000;
      snprintf(out, n, "%lud %02lu:%02lu", (unsigned long)(s / 86400), (unsigned long)((s / 3600) % 24), (unsigned long)((s / 60) % 60));
      break;
    }
    case INFO_HEAP: snprintf(out, n, "%luk / %luk psram", (unsigned long)(ESP.getFreeHeap() / 1024), (unsigned long)(ESP.getFreePsram() / 1024)); break;
    case INFO_TRACKS: snprintf(out, n, "%u", (unsigned)library::count()); break;
    case INFO_SYNC: strlcpy(out, syncgrp::stateName(), n); break;
    case REBOOT_NOW: strlcpy(out, "< press >", n); break;
    case EXIT: strlcpy(out, "< press >", n); break;
    default: out[0] = 0;
  }
}

}  // namespace menu
