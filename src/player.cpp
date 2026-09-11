#include "player.h"

#include <Audio.h>
#include <SD.h>
#include <esp_task_wdt.h>

#include <atomic>

#include "config.h"
#include "library.h"
#include "sync.h"

namespace {

Audio g_audio(I2S_NUM_0);
QueueHandle_t g_cmd = nullptr;
TaskHandle_t g_pump = nullptr;
SemaphoreHandle_t g_busLock = nullptr;   // SD (pump) vs display (UI) on the shared SPI bus

struct Cmd {
  enum Type : uint8_t { Play, Stop, Volume } type;
  int track;
  uint8_t vol;
};

// pump -> UI signals
std::atomic<bool> g_evtEof{false};
std::atomic<bool> g_evtOpenFail{false};
std::atomic<bool> g_evtOpened{false};
std::atomic<int> g_pumpTrack{-1};        // what the pump is on (authoritative)
std::atomic<int> g_pumpTried{-1};        // last track the pump attempted to open
std::atomic<bool> g_rebootAtWrap{false};  // supervisor asks: stop at the end of the playlist
std::atomic<bool> g_rebootPending{false}; // pump answers: playlist ended, nothing playing

// UI-task state
PlayerState g_state = PlayerState::NoSd;
int g_track = -1;
uint8_t g_volume = cfg::VOL_DEFAULT;
uint32_t g_gen = 1;
uint32_t g_lastPos = 0, g_lastProgressMs = 0, g_stallRestarts = 0;
uint32_t g_failed = 0;         // consecutive open failures
uint32_t g_sweepAt = 0;
char g_codec[8] = "";

void onAudioEvent(Audio::msg_t m) {   // pump-task context
  switch (m.e) {
    case Audio::evt_eof:
      g_evtEof = true;
      break;
    case Audio::evt_info:
      log_d("[audio] %s", m.msg ? m.msg : "");
      break;
    case Audio::evt_log:
      log_w("[audio] %s", m.msg ? m.msg : "");
      break;
    default:
      break;
  }
}

bool openTrack(int track) {   // pump-task context, bus lock held by caller
  if (track < 0 || track >= (int)library::count()) return false;
  char path[cfg::MAX_NAME + 2];
  snprintf(path, sizeof(path), "/%s", library::at(track).name);
  g_audio.stopSong();
  g_pumpTried = track;
  bool ok = g_audio.connecttoFS(SD, path);
  log_i("open [%d] %s -> %s", track, path, ok ? "ok" : "FAILED");
  g_pumpTrack = ok ? track : -1;
  if (ok) g_evtOpened = true; else g_evtOpenFail = true;
  return ok;
}

int nextOf(int track) {
  size_t n = library::count();
  if (n == 0) return -1;
  return (track + 1) % (int)n;
}

void pumpLoop(void*) {
  esp_task_wdt_add(nullptr);
  for (;;) {
    esp_task_wdt_reset();
    Cmd c;
    bool havePlay = false, haveStop = false;
    int playTrack = -1;
    while (xQueueReceive(g_cmd, &c, 0) == pdTRUE) {
      switch (c.type) {
        case Cmd::Play:   havePlay = true; haveStop = false; playTrack = c.track; break;
        case Cmd::Stop:   haveStop = true; havePlay = false; break;
        case Cmd::Volume: g_audio.setVolume(c.vol); break;
      }
    }
    xSemaphoreTake(g_busLock, portMAX_DELAY);
    if (haveStop) {
      g_audio.stopSong();
      g_pumpTrack = -1;
    }
    if (havePlay) openTrack(playTrack);
    g_audio.loop();
    // Gapless-ish advance: the moment the decoder signals end of file, open the next one
    // right here instead of waiting for a UI tick. The UI learns about it from g_pumpTrack.
    if (g_evtEof.exchange(false)) {
      int cur = g_pumpTrack.load();
      int nxt = nextOf(cur);
      if (nxt == 0 && g_rebootAtWrap.load()) {
        // end of the playlist with a reboot requested: leave the card and the codec quiet,
        // the supervisor restarts the box and the boot path starts from the first media.
        g_audio.stopSong();
        g_pumpTrack = -1;
        g_rebootPending = true;
      } else if (nxt >= 0) {
        openTrack(nxt);
      }
    }
    xSemaphoreGive(g_busLock);
    vTaskDelay(1);
  }
}

void send(Cmd c) {
  if (g_cmd) xQueueSend(g_cmd, &c, 0);
}

void startTrack(int track) {
  if (track < 0 || track >= (int)library::count()) return;
  g_track = track;
  g_state = PlayerState::Starting;
  g_lastPos = 0;
  g_lastProgressMs = millis();
  g_evtOpenFail = false;
  g_evtOpened = false;
  send(Cmd{Cmd::Play, track, 0});
  g_gen++;
}

}  // namespace

namespace player {

void begin(const AudioPins& pins, uint8_t volumePct) {
  g_volume = volumePct > 100 ? 100 : volumePct;
  g_busLock = xSemaphoreCreateMutex();
  g_cmd = xQueueCreate(8, sizeof(Cmd));

  Audio::audio_info_callback = onAudioEvent;
  // Core split: decode task on core 1, pump (SD reads + audio.loop) on core 0.
  // setAudioTaskCore MUST precede setPinout — on a live task it deletes and recreates a
  // static-TCB task and IDLE0 later crashes in uxListRemove (see docs/DESIGN.md).
  g_audio.setAudioTaskCore(1);
  g_audio.setPinout(pins.bclk, pins.lrck, pins.dout, pins.mclk);
  g_audio.setOutputSampleRate(Audio::SR_48000);   // one clock for every file: no I2S reconfig
  g_audio.setVolume(21);                           // engine wide open, the codec holds the level
  log_i("audio engine %s pins bclk=%d lrck=%d dout=%d mclk=%d", g_audio.getVersion(), pins.bclk,
        pins.lrck, pins.dout, pins.mclk);

  xTaskCreatePinnedToCore(pumpLoop, "hp_pump", 8192, nullptr, 3, &g_pump, 0);
  g_state = library::count() ? PlayerState::Empty : (library::mounted() ? PlayerState::Empty : PlayerState::NoSd);
  g_gen++;
}

void onLibraryChanged() {
  g_failed = 0;
  if (!library::mounted()) {
    send(Cmd{Cmd::Stop, 0, 0});
    g_state = PlayerState::NoSd;
    g_track = -1;
    g_gen++;
    return;
  }
  if (library::count() == 0) {
    send(Cmd{Cmd::Stop, 0, 0});
    g_state = PlayerState::Empty;
    g_track = -1;
    g_gen++;
    return;
  }
  startTrack(0);
}

void play(int track) { startTrack(track); }
void next() { if (library::count()) startTrack((g_track + 1) % (int)library::count()); }
void prev() { if (library::count()) startTrack((g_track - 1 + (int)library::count()) % (int)library::count()); }

void stop() {
  send(Cmd{Cmd::Stop, 0, 0});
  g_state = library::count() ? PlayerState::Empty : PlayerState::NoSd;
  g_gen++;
}

void setVolume(uint8_t pct) {
  g_volume = pct > 100 ? 100 : pct;
  codec::setVolume(g_volume);
  g_gen++;
}

void tick() {
  uint32_t now = millis();
  if (g_state == PlayerState::NoSd || g_state == PlayerState::Empty) return;

  // Follow the pump's own advances (end of file -> next track).
  int pt = g_pumpTrack.load();
  if (pt >= 0 && pt != g_track && g_state == PlayerState::Playing) {
    g_track = pt;
    g_lastPos = 0;
    g_lastProgressMs = now;
    g_gen++;
    const Track& t = library::at(g_track);
    syncgrp::reportTrack(t.index, t.name, g_audio.getAudioFileDuration() * 1000UL);
  }

  if (g_evtOpenFail.exchange(false)) {
    g_failed++;
    if (g_failed >= library::count()) {
      log_e("every track failed to open — sweep again in %lus", cfg::OPEN_FAIL_SWEEP_MS / 1000);
      g_state = PlayerState::Failed;
      g_sweepAt = now + cfg::OPEN_FAIL_SWEEP_MS;
      g_gen++;
      // A card yanked mid-play looks exactly like this: let the supervisor re-probe it.
      if (!library::cardAlive()) {
        library::unmount();
        onLibraryChanged();
      }
      return;
    }
    // advance from the track that actually failed (the pump may already have moved on)
    startTrack(nextOf(g_pumpTried.load() >= 0 ? g_pumpTried.load() : g_track));
    return;
  }

  switch (g_state) {
    case PlayerState::Starting:
      if (g_evtOpened.exchange(false)) {
        g_failed = 0;
        g_state = PlayerState::Playing;
        strlcpy(g_codec, g_audio.getCodecname(), sizeof(g_codec));
        g_lastProgressMs = now;
        g_gen++;
        const Track& t = library::at(g_track);
        syncgrp::reportTrack(t.index, t.name, g_audio.getAudioFileDuration() * 1000UL);
      } else if (now - g_lastProgressMs > 10000) {
        log_e("open timeout on [%d]", g_track);
        g_evtOpenFail = true;
      }
      break;

    case PlayerState::Playing: {
      uint32_t pos = g_audio.getAudioCurrentTime();
      if (pos != g_lastPos) {
        g_lastPos = pos;
        g_lastProgressMs = now;
        g_stallRestarts = 0;
      }
      syncgrp::reportPosition(pos * 1000UL, g_audio.isRunning());
      if (g_rebootPending.load()) break;   // playlist ended on purpose, reboot is imminent
      if (!g_audio.isRunning() && g_pumpTrack.load() < 0) {
        // Decoder gave up without an eof (corrupt file): move on.
        log_w("decoder stopped on [%d] without eof", g_track);
        startTrack(nextOf(g_track));
        break;
      }
      uint32_t frozen = now - g_lastProgressMs;
      if (frozen > cfg::STALL_REBOOT_MS) {
        log_e("playback frozen %lus — rebooting", frozen / 1000);
        delay(100);
        ESP.restart();
      } else if (frozen > cfg::STALL_MS && g_stallRestarts == 0) {
        log_e("playback frozen %lus — restarting track", frozen / 1000);
        g_stallRestarts++;
        startTrack(g_track);
      }
      break;
    }

    case PlayerState::Failed:
      if ((int32_t)(now - g_sweepAt) >= 0) {
        g_failed = 0;
        startTrack(g_track >= 0 ? g_track : 0);
      }
      break;

    default:
      break;
  }
}

PlayerSnapshot snapshot() {
  PlayerSnapshot s;
  s.state = g_state;
  s.track = g_track;
  s.volume = g_volume;
  s.generation = g_gen;
  if (g_state == PlayerState::Playing) {
    s.posSec = g_audio.getAudioCurrentTime();
    s.durSec = g_audio.getAudioFileDuration();
  }
  strlcpy(s.codec, g_codec, sizeof(s.codec));
  return s;
}

const char* stateName(PlayerState s) {
  switch (s) {
    case PlayerState::NoSd:     return "no sd card";
    case PlayerState::Empty:    return "no media";
    case PlayerState::Starting: return "starting";
    case PlayerState::Playing:  return "playing";
    case PlayerState::Failed:   return "failed";
  }
  return "?";
}

void requestRebootAtWrap() { g_rebootAtWrap = true; }
bool rebootPending() { return g_rebootPending.load(); }

TaskHandle_t pumpTask() { return g_pump; }

// The UI needs the same bus lock to push its sprite over SPI.
}  // namespace player

SemaphoreHandle_t player_busLock() { return g_busLock; }
