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
| `ui` | one 320x240 sprite in PSRAM, list centred on the current track, marquee title, time chip, tap-to-play mapping, pushed under the bus lock |
| `menu` | center-button menu, edits `Settings`, applies immediately |
| `settings` | NVS, writes coalesced 2 s after the last change |
| `supervisor` | SD hot-plug, idle dim, LEDs, scheduled reboot at the playlist wrap, health log, task watchdog |
| `sync` | stub seam for the Nowde slave role (see below) |
| `usbdrive` | TinyUSB composite: CDC (log + console + flashing) and MSC exposing the card by sectors; offer / active state machine |
| `console` | one-line commands on the CDC for the bench (`usb`, `eject`, `play N`, ...) |

## Decisions

- **Engine: ESP32-audioI2S** *(T)*, pinned by tag. Real duration and position for every
  format, seek, its own decode task. It needs Arduino core 3 and PSRAM, so the **Core
  Basic is out** of this engine. If a Basic is ever needed, add an ESP8266Audio backend
  behind the same `player` API (it decodes mp3/aac/flac without PSRAM but gives no
  duration for mp3, that would have to be parsed from the frames).
- **CoreS3 SE only** *(T, 2026-09-11 evening)*: the Fire env was dropped. Pins still come
  from M5Unified's M-Bus table, so re-adding a Fire env is a two-line change if wanted.
- **Look** *(T)*: red / yellow / green / cyan on black, "retro-future" but light: no frames,
  plain labels, a faint fill under the playing row, the time in a small chip on that row's
  top edge so the title keeps the whole width (the row above is clipped to make room).
  Fonts: Share Tech for text (Latin-1 so accents render), Share Tech Mono for numbers,
  Orbitron for the header. VT323 was tried first and judged too hard; M5GFX's built-in
  DejaVu fonts are ASCII-only, which is why the very first build drew squares for `é`.
- **Touch**: the CoreS3 SE screen is a touch panel. Tap a list row to play it, tap a menu
  entry to select it and again to act. Drag the list to browse: pixel-smooth, row snap on
  release, eased recentre after 6 s idle or on a track change; no wrap-around (Thomas: no
  last file before 01, no first file after the last), blank rows beyond the ends, the drag
  stops there. The menu is the same wheel with the selected entry in the centre.
  M5Unified keeps mapping the strip under the LCD (y ≥ 240) to BtnA/B/C, so the three
  button roles are unchanged. The header's middle dot in `HPLAYER·0` is drawn by hand:
  Orbitron's TTF has no U+00B7 glyph and the generator emits a tofu box for it.
- **Loop: whole playlist, no gap** *(T)*. The pump task opens the next file the moment the
  decoder reports end of file, without waiting for a UI tick.
- **Volume in the codec**, not in the engine: the ES8388 DAC attenuator keeps the full
  digital resolution; the engine runs wide open.
- **Fixed 48 kHz output clock**: the engine resamples every file, so the I2S clock is never
  reconfigured between tracks (no click, no codec re-lock).
- **The card is never written**. Logs go to serial only; power cuts cannot corrupt media.
- **Extras in v0.1** *(T)*: idle backlight dim, optional uptime-based reboot at the end of the playlist (the last media finishes, the box reboots, playback resumes from the first),
  module LEDs as status, Nowde hooks reserved (`sync.h`, numeric-prefix cue index).

## Hardware truths (from WaveHopper's CoreS3 work, `~/Bakery/WaveHopper/players/m5cores3`)

- Module Audio I2S on the CoreS3 (switch B): BCLK GPIO0, LRCK GPIO6, DOUT GPIO13, **MCLK
  GPIO7, mandatory**. We take them from `M5.getPin(mbus_pin2x)` (bus pins 24 / 21 / 23 / 22)
  so the Fire gets its own: BCLK 13, LRCK 12, DOUT 15, MCLK 0 (switch A swaps 22 and 24).
- Probe the module at **0x33** (STM32 helper) only: the CoreS3's BMM150 answers at 0x10.
- Never `Wire.begin()` on the Core's internal I2C pins: the M5 wrapper library does, and it
  detaches `M5.In_I2C` (touch controller) silently. We talk to the codec through `M5.In_I2C`.
- The stock ES8388 init leaves a mic monitor path open (hiss on a headset): mixers forced to
  DAC-only (`0x27`/`0x2A` = `0x90`), ADC and micbias powered down (`0x03` = `0xFF`).
- `setAudioTaskCore()` **before** `setPinout()`, never on a live task (static-TCB delete and
  recreate → intermittent IDLE0 panic). Decode task and `audio.loop()` on opposite cores.
- `cfg.internal_spk = false` and `internal_mic = false` in `M5.config()`, or M5Unified's own
  speaker driver fights for the I2S peripheral on the CoreS3.
- The **FT6336 touch controller** falls back to a low-rate monitor mode after a few idle
  seconds (M5GFX leaves the default) and quick taps after idling get lost: write `0x00` to
  its CTRL register `0x86` (I2C `0x38` on `M5.In_I2C`) once at boot to keep it active. The
  wake logic also accepts any touched sample, not only the first (`isPressed`).
- **Batteries and the DIN Base** (Thomas, 2026-09-13): with the DIN Base's own 500 mAh cell
  feeding the bus, the Core's AXP2101 still sees VBUS when the 9 V PSU is cut, so "on
  battery" is not detectable from software, and a software power-off would need a button
  press to come back (a steady VBUS gives no insertion edge). Decision: remove both cells
  (base and Core) for exhibition units. Fallback if a Core cell must stay: `M5.Power.setBatteryCharge(false)`
  so it drains once and stays empty.
- The **CoreS3 SE has no IMU** (verified 2026-09-12: `M5.Imu.getType()` = `imu_none`), unlike
  the CoreS3 with its BMI270. The motion-wake code in `supervisor` stays dormant on the SE.

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
6. (Fire, if ever re-added, switch A: M5Unified's table gives BCLK GPIO13, LRCK GPIO12,
   DOUT GPIO15, MCLK GPIO0 — GPIO0 is MCLK-capable on a classic ESP32.)

Reboot-at-playlist-end proven 2026-09-12 08:22 on the bench flavour (`HP_TEST_REBOOT_MS=600000`):
last track ended at 873 s uptime, "playlist ended, scheduled reboot" logged, clean reboot,
playback resumed on the first file.

Bench log 2026-09-11 (CoreS3 SE, 30 GB card, 6 files: wav, flac, m4a, ogg, opus, mp3):
boot to first sound in under a second, module found, position tracks real time, heap flat
at 192 KB free across the run, transitions open the next file at the end-of-file event.

## USB drive mode

- `platformio.ini` switches the CoreS3 SE from the hardware serial-JTAG (`ARDUINO_USB_MODE=1`,
  what the board manifest sets) to TinyUSB OTG (`=0`), with `ARDUINO_USB_CDC_ON_BOOT=0` so
  the descriptors (VID 0x303A, PID 0x8001, Hemisphere / HPlayer0 / MAC serial) are set before
  `USB.begin()`. `USBCDC::setDebugOutput(true)` routes the `log_*` lines to the CDC. Uploads
  use PlatformIO's 1200-baud touch (`use_1200bps_touch`, `wait_for_upload_port`), as Nowde.
- The MSC LUN is created at boot with the card's sector count and **media absent**: the
  computer sees a card reader with no card. Entering drive mode stops the player, then flips
  `mediaPresent(true)`; the host polls TEST UNIT READY and mounts. Leaving flips it back,
  then `SD.end()` + `SD.begin()` + rescan: FatFs must not trust anything it cached before
  the computer rewrote the card. Enter needs the card that was there at boot (the LUN size
  is fixed at `begin`); a card inserted later needs a reboot before drive mode.
- Sector callbacks run in the TinyUSB task under the same SPI bus lock as the audio pump
  and the display push. TinyUSB hands 4 KB chunks (`CONFIG_TINYUSB_MSC_BUFSIZE`); the SD
  library's `readRAW` is one command per sector, so the module declares the diskio layer's
  `ff_sd_read` / `ff_sd_write` (non-static in `sd_diskio.cpp`) and moves 8 sectors per
  command. The FatFs drive number is found by matching `sdcard_num_sectors(d)`.
- Host detection = `ARDUINO_USB_STARTED_EVENT` (enumeration). A charger never enumerates,
  so the offer only appears on a computer; it times out after 15 s. Leaving happens on the
  host's eject (`onStartStop` with `load_eject`), on unplug / suspend, on the console's
  `eject`, or by holding MENU on the drive screen.
- Bench 2026-09-11/12 on the laptop: enumerates as `303a:8001 Hemisphere HPlayer0`, `/dev/sda`
  "HPlayer microSD" 0 B until drive mode, then 29.8 GB with the FAT partition mounted;
  single-sector commands gave 309 kB/s read / 177 kB/s write; 8-sector commands through a
  **DMA-capable bounce buffer** give 503 kB/s read / 584 kB/s write, zero errors. Passing
  TinyUSB's own buffer straight to `ff_sd_read` looked fine locally (`bench` reads 2 MB at
  1.5 MB/s) but every host session stalled after exactly 3244 KB with no error on either
  side — the SPI driver wants DMA-able, aligned memory for multi-block transfers. Ejecting
  (console) remounted and rescanned to 8 files and playback restarted.
- While the computer holds the card the serial output is muted (Nowde's finding on this
  TinyUSB build: two busy IN endpoints lose transfers) and the log goes to an 8 KB RAM
  ring the console prints with `dump`. `bench` measures local SD read throughput.
- Serial tooling gotcha: a second process opening the CDC port drops DTR on close, and
  `USBCDC::write` silently discards everything while DTR is low, so the log goes mute.
  One reader owns the port and relays commands (`scripts/serial_capture.py` + `serial_cmd.py`).

## Nowde integration (after the Biennale delivery)

The CoreS3 is an ESP32-S3: it can run the Nowde **slave role natively** instead of hanging a
Nowde node off a USB port. Plan: link `ESPNowMeshClock`, receive `MediaSyncPacket`
(layer, index, positionMs, state, meshTimestamp) exactly as `receiver_mode.cpp` does, and
feed `syncgrp::poll()`: index → `library::findByIndex`, position → `setAudioPlayTime` when
the drift exceeds a threshold, state 0 → stop. Rate trimming is not available in the
engine, so the servo is "hard resync beyond N ms", which is fine for audio-only slaves.
A later step makes an HPlayer0 a **master** too (broadcast its own position), giving a
Pi-less synced audio fleet. Content convention stays HPlayer2's: `01_…` carries index 1.
