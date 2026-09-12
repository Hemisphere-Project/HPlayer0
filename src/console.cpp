#include "console.h"

#include <M5Unified.h>
#include <USBCDC.h>

#include "config.h"
#include "library.h"
#include "player.h"
#include "settings.h"
#include "usbdrive.h"

USBCDC& usbdrive_cdc();
bool usbdrive_quiet();
void usbdrive_dump(Print& out);
void console_setVolume(uint8_t pct);   // main.cpp: keeps the settings in step

namespace {
char g_line[64];
size_t g_len = 0;

Print& sink() {
  static struct : Print { size_t write(uint8_t) override { return 1; } } null;
  return usbdrive_quiet() ? (Print&)null : (Print&)usbdrive_cdc();
}

void run(char* line) {
  char* arg = strchr(line, ' ');
  if (arg) *arg++ = 0;
  Print& out = sink();
  if (!strcmp(line, "help")) {
    out.println("help info bench dump next prev play N stop vol N usb eject reboot");
  } else if (!strcmp(line, "info")) {
    PlayerSnapshot s = player::snapshot();
    out.printf("fw %s state=%s track=%d/%u pos=%lu/%lu vol=%u heap=%lu psram=%lu usb=%d host=%d ioerr=%lu imu=%d\n",
               HP_VERSION, player::stateName(s.state), s.track, (unsigned)library::count(),
               (unsigned long)s.posSec, (unsigned long)s.durSec, s.volume, (unsigned long)ESP.getFreeHeap(),
               (unsigned long)ESP.getFreePsram(), (int)usbdrive::state(), (int)usbdrive::hostConnected(), (unsigned long)usbdrive::ioErrors(), (int)M5.Imu.getType());
    for (size_t i = 0; i < library::count(); ++i) out.printf("  [%u] %s\n", (unsigned)i, library::at(i).name);
  } else if (!strcmp(line, "dump")) {
    usbdrive_dump(out);
  } else if (!strcmp(line, "bench")) {
    usbdrive::bench(out);
  } else if (!strcmp(line, "next")) {
    player::next();
  } else if (!strcmp(line, "prev")) {
    player::prev();
  } else if (!strcmp(line, "play") && arg) {
    player::play(atoi(arg));
  } else if (!strcmp(line, "stop")) {
    player::stop();
  } else if (!strcmp(line, "vol") && arg) {
    console_setVolume((uint8_t)constrain(atoi(arg), 0, 100));
  } else if (!strcmp(line, "usb")) {
    usbdrive::enter();
  } else if (!strcmp(line, "eject")) {
    usbdrive::leave();
  } else if (!strcmp(line, "reboot")) {
    out.println("rebooting");
    delay(100);
    ESP.restart();
  } else {
    out.printf("? %s\n", line);
  }
}
}  // namespace

namespace console {
void tick() {
  USBCDC& in = usbdrive_cdc();
  while (in.available()) {
    int c = in.read();
    if (c < 0) break;
    if (c == '\r' || c == '\n') {
      if (g_len) { g_line[g_len] = 0; run(g_line); g_len = 0; }
    } else if (g_len < sizeof(g_line) - 1) {
      g_line[g_len++] = (char)c;
    }
  }
}
}  // namespace console
