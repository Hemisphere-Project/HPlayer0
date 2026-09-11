# HPlayer0 design notes

Rewrite started 2026-09-11 for a Biennale de Lyon 2026 delivery (one CoreS3 SE + Module
Audio). Deadline Monday 2026-09-14. Decisions taken with Thomas that day are marked *(T)*.

## Architecture

```
 UI task (Arduino loop, core 1)            pump task (core 0)           decode task (core 1)
 ─────────────────────────────            ──────────────────           ────────────────────
 M5.update / buttons / menu     cmd queue  audio.loop()                 ESP32-audioI2S
 player::tick (state machine)  ─────────▶ SD reads, connecttoFS   ───▶ codec -> I2S -> ES8388
 supervisor / settings / ui                eof -> open next track
 sprite -> pushSprite  ◀────── bus lock ──▶ (SD and LCD share one SPI bus)
```

| Module | Role |
|--------|------|
| `library` | read-only scan of the SD root into a fixed table, sorted, numeric prefix = cue index |
| `player` | ESP32-audioI2S 3.4.7 behind a command queue; loops the table; failure = next track |
| `codec` | ES8388 over `M5.In_I2C` (vendored driver in `lib/ES8388`), STM32 helper for LEDs / jack |
| `ui` | one 320x240 sprite in PSRAM, list centred on the current track, pushed under the bus lock |
| `menu` | center-button menu, edits `Settings`, applies immediately |
| `settings` | NVS, writes coalesced 2 s after the last change |
| `supervisor` | SD hot-plug, idle dim, LEDs, scheduled reboot, health log, task watchdog |
| `sync` | stub seam for the Nowde slave role (see below) |

## Decisions

- **Engine: ESP32-audioI2S** *(T)*, pinned by tag. Real duration and position for every
  format, seek, its own decode task. It needs Arduino core 3 and PSRAM, so the **Core
  Basic is out** of this engine. If a Basic is ever needed, add an ESP8266Audio backend
  behind the same `player` API (it decodes mp3/aac/flac without PSRAM but gives no
  duration for mp3, that would have to be parsed from the frames).
- **Loop: whole playlist, no gap** *(T)*. The pump task opens the next file the moment the
  decoder reports end of file, without waiting for a UI tick.
- **Volume in the codec**, not in the engine: the ES8388 DAC attenuator keeps the full
  digital resolution; the engine runs wide open.
- **Fixed 48 kHz output clock**: the engine resamples every file, so the I2S clock is never
  reconfigured between tracks (no click, no codec re-lock).
- **The card is never written**. Logs go to serial only; power cuts cannot corrupt media.
- **Extras in v0.1** *(T)*: idle backlight dim, optional uptime-based reboot between tracks,
  module LEDs as status, Nowde hooks reserved (`sync.h`, numeric-prefix cue index).

## Hardware truths (from WaveHopper's CoreS3 work, `~/Bakery/WaveHopper/players/m5cores3`)

- Module Audio I2S on the CoreS3 (switch B): BCLK GPIO0, LRCK GPIO6, DOUT GPIO13, **MCLK
  GPIO7, mandatory**. We take them from `M5.getPin(mbus_pin2x)` so the Fire gets its own.
- Probe the module at **0x33** (STM32 helper) only: the CoreS3's BMM150 answers at 0x10.
- Never `Wire.begin()` on the Core's internal I2C pins: the M5 wrapper library does, and it
  detaches `M5.In_I2C` (touch controller) silently. We talk to the codec through `M5.In_I2C`.
- The stock ES8388 init leaves a mic monitor path open (hiss on a headset): mixers forced to
  DAC-only (`0x27`/`0x2A` = `0x90`), ADC and micbias powered down (`0x03` = `0xFF`).
- `setAudioTaskCore()` **before** `setPinout()`, never on a live task (static-TCB delete and
  recreate → intermittent IDLE0 panic). Decode task and `audio.loop()` on opposite cores.
- `cfg.internal_spk = false` and `internal_mic = false` in `M5.config()`, or M5Unified's own
  speaker driver fights for the I2S peripheral on the CoreS3.

## Robustness

- Task watchdog (30 s) on the pump task and the UI task: a wedged SD or I2C access reboots.
- Playback position frozen 15 s → restart the track; 45 s → reboot.
- Open failure → next track; every track failed → 5 s sweep; card gone → unmount, remount
  loop every 2 s, rescan, restart from the first track.
- Health log every minute: uptime, state, track, position, heap, min heap, PSRAM.
- Reset reason on the boot screen, so a field "it rebooted" report names the culprit.

## Bench plan (CoreS3 SE + Module Audio on the laptop)

1. Flash `cores3`, card with one wav + one mp3 + one flac: boot screen, list, sound.
2. Volume range and the codec level (`setDACVolume`): pick a default that does not clip the
   venue input; check for hiss with nothing playing.
3. Pull the card while playing, reinsert: NO SD, then back to track 1.
4. Files that fail (rename a .txt to .mp3): skipped, others still loop.
5. Overnight soak with the health log captured; heap must be flat.
6. Fire, switch A: same list, MCLK on a classic ESP32 is only valid on GPIO0/1/3 — verify
   the M-Bus pin the module uses.

## Nowde integration (after the Biennale delivery)

The CoreS3 is an ESP32-S3: it can run the Nowde **slave role natively** instead of hanging a
Nowde node off a USB port. Plan: link `ESPNowMeshClock`, receive `MediaSyncPacket`
(layer, index, positionMs, state, meshTimestamp) exactly as `receiver_mode.cpp` does, and
feed `syncgrp::poll()`: index → `library::findByIndex`, position → `setAudioPlayTime` when
the drift exceeds a threshold, state 0 → stop. Rate trimming is not available in the
engine, so the servo is "hard resync beyond N ms", which is fine for audio-only slaves.
A later step makes an HPlayer0 a **master** too (broadcast its own position), giving a
Pi-less synced audio fleet. Content convention stays HPlayer2's: `01_…` carries index 1.
