#include "settings.h"

#include <Preferences.h>

#include "config.h"

namespace {
Preferences g_prefs;
bool g_dirty = false;
uint32_t g_saveAt = 0;

void write(const Settings& s) {
  if (!g_prefs.begin("hp0", false)) {
    log_e("nvs open failed");
    return;
  }
  g_prefs.putUChar("vol", s.volume);
  g_prefs.putUChar("bright", s.brightness);
  g_prefs.putUShort("dim", s.dimAfterS);
  g_prefs.putUChar("reboot", s.rebootEveryH);
  g_prefs.putUChar("led", s.ledLevel);
  g_prefs.end();
  log_i("settings saved: vol=%u bright=%u dim=%us reboot=%uh led=%u", s.volume, s.brightness,
        s.dimAfterS, s.rebootEveryH, s.ledLevel);
}
}  // namespace

namespace settings {

void load(Settings& s) {
  if (!g_prefs.begin("hp0", true)) {
    log_w("nvs namespace missing, defaults");
    return;
  }
  s.volume       = g_prefs.getUChar("vol", s.volume);
  s.brightness   = g_prefs.getUChar("bright", s.brightness);
  s.dimAfterS    = g_prefs.getUShort("dim", s.dimAfterS);
  s.rebootEveryH = g_prefs.getUChar("reboot", s.rebootEveryH);
  s.ledLevel     = g_prefs.getUChar("led", s.ledLevel);
  g_prefs.end();
  if (s.volume > 100) s.volume = 100;
  if (s.brightness < cfg::BRIGHT_MIN) s.brightness = cfg::BRIGHT_MIN;
  if (s.ledLevel > 100) s.ledLevel = 100;
}

void markDirty() {
  g_dirty = true;
  g_saveAt = millis() + cfg::NVS_SAVE_DELAY_MS;
}

void tick(const Settings& s) {
  if (g_dirty && (int32_t)(millis() - g_saveAt) >= 0) {
    g_dirty = false;
    write(s);
  }
}

void flushNow(const Settings& s) {
  g_dirty = false;
  write(s);
}

}  // namespace settings
