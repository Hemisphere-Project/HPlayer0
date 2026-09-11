#include "supervisor.h"

#include <M5Unified.h>
#include <esp_task_wdt.h>

#include "codec.h"
#include "config.h"
#include "library.h"
#include "player.h"

namespace {
Settings* g_s = nullptr;
uint32_t g_lastInput = 0;
bool g_dimmed = false;
uint32_t g_sdRetryAt = 0, g_sdCheckAt = 0, g_healthAt = 0;
uint32_t g_ledColor = 0xFFFFFFFF;

void leds(uint32_t rgb) {
  if (rgb == g_ledColor) return;
  g_ledColor = rgb;
  codec::setLeds(rgb);
}

void rescan() {
  SemaphoreHandle_t lock = player_busLock();
  if (lock) xSemaphoreTake(lock, portMAX_DELAY);
  library::scan();
  if (lock) xSemaphoreGive(lock);
  player::onLibraryChanged();
}
}  // namespace

namespace supervisor {

void begin(Settings* s) {
  g_s = s;
  g_lastInput = millis();
  // Task watchdog: the pump task subscribes itself, the UI (loop) task here. A wedged SD
  // access or a hung I2C transaction then costs a reboot instead of a silent player.
  esp_task_wdt_config_t twdt = {
      .timeout_ms = cfg::PUMP_WDT_S * 1000,
      .idle_core_mask = 0,
      .trigger_panic = true,
  };
  esp_task_wdt_reconfigure(&twdt);
  esp_task_wdt_add(nullptr);
  codec::setLedBrightness(g_s->ledLevel);
}

bool noteInput() {
  g_lastInput = millis();
  if (g_dimmed) {
    g_dimmed = false;
    M5.Display.setBrightness(g_s->brightness);
    return true;
  }
  return false;
}

void tick(bool menuOpen) {
  uint32_t now = millis();
  esp_task_wdt_reset();

  // --- SD hot-plug -------------------------------------------------------------------
  if (!library::mounted()) {
    if ((int32_t)(now - g_sdRetryAt) >= 0) {
      g_sdRetryAt = now + cfg::SD_RETRY_MS;
      SemaphoreHandle_t lock = player_busLock();
      if (lock) xSemaphoreTake(lock, portMAX_DELAY);
      bool ok = library::mount();
      if (lock) xSemaphoreGive(lock);
      if (ok) rescan();
    }
  } else if ((int32_t)(now - g_sdCheckAt) >= 0) {
    g_sdCheckAt = now + 5000;
    SemaphoreHandle_t lock = player_busLock();
    if (lock) xSemaphoreTake(lock, portMAX_DELAY);
    bool alive = library::cardAlive();
    if (!alive) library::unmount();
    if (lock) xSemaphoreGive(lock);
    if (!alive) {
      player::onLibraryChanged();
      g_sdRetryAt = now + cfg::SD_RETRY_MS;
    }
  }

  // --- idle dim ------------------------------------------------------------------------
  if (g_s->dimAfterS && !g_dimmed && !menuOpen && now - g_lastInput > (uint32_t)g_s->dimAfterS * 1000UL) {
    g_dimmed = true;
    M5.Display.setBrightness(cfg::DIM_LEVEL);
  }

  // --- module LEDs -----------------------------------------------------------------------
  PlayerSnapshot s = player::snapshot();
  if (menuOpen) leds(0x0020FF);
  else switch (s.state) {
      case PlayerState::Playing:  leds(0x00FF20); break;
      case PlayerState::Starting: leds(0x00A0FF); break;
      case PlayerState::Empty:    leds(0xFF6000); break;
      case PlayerState::NoSd:     leds(0xFF0000); break;
      case PlayerState::Failed:   leds(0xFF0040); break;
    }

  // --- scheduled reboot, only between two tracks ----------------------------------------
  bool boundary = player::atTrackBoundary();
  if (g_s->rebootEveryH && boundary && now > (uint32_t)g_s->rebootEveryH * 3600000UL) {
    log_w("scheduled reboot after %luh uptime", (unsigned long)(now / 3600000UL));
    settings::flushNow(*g_s);
    delay(100);
    ESP.restart();
  }

  // --- health log ------------------------------------------------------------------------
  if ((int32_t)(now - g_healthAt) >= 0) {
    g_healthAt = now + cfg::HEALTH_LOG_MS;
    log_i("[health] up=%lus state=%s track=%d pos=%lu/%lu heap=%lu min=%lu psram=%lu",
          (unsigned long)(now / 1000), player::stateName(s.state), s.track, (unsigned long)s.posSec,
          (unsigned long)s.durSec, (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap(),
          (unsigned long)ESP.getFreePsram());
  }
}

}  // namespace supervisor
