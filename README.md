# HPlayer0

**Looping audio player for M5Stack Core + Module Audio (M144), from a microSD card.**

Drop audio files on a card, plug the card in, power the box: it plays every supported file
in order, forever. The screen shows the media list centred on the playing track with its
position and duration; the three buttons are **volume -**, **menu**, **volume +**. Built for
exhibitions that run every day for months: read-only card, watchdogs, stall detection,
SD hot-plug, optional scheduled reboot between two tracks.

Hemisphere Project · GPL-3.0 · repo `github.com/Hemisphere-Project/HPlayer0`.
The 2024 prototype (WAV + LoRa + USB-MIDI + Bluetooth) is parked on branch `proto-2024`.

## Hardware

| Core | Env | Module Audio pin switch | Buttons |
|------|-----|-------------------------|---------|
| M5Stack **CoreS3** / **CoreS3 SE** | `cores3` | **B** | touch strip under the screen |
| M5Stack **Fire** | `fire` | **A** | physical |
| M5Stack Core Basic | — | — | no PSRAM: not supported by this engine, see `docs/DESIGN.md` |

Audio comes out of the module's **TRRS headphone jack** (ES8388 DAC, headphone amplifier),
which is what feeds the venue amplifier. The module's three RGB LEDs show the state:
green playing, blue starting or menu, orange no media, red no card or failure.

## Media

- Files in the **root** of the card (subfolders are ignored), FAT32 / exFAT.
- Formats: **mp3, wav, aac, m4a, flac, ogg (vorbis), opus**. Any sample rate, resampled
  to a fixed 48 kHz output.
- Order: file names sorted case-insensitively. Name them `01_…`, `02_…` to control it.
  The numeric prefix is also the cue index a future Nowde sync will use, like HPlayer2.
- Hidden files and macOS `._` forks are skipped. The card is never written.

## Buttons and menu

- **Left / right**: volume in steps of 5, hold to repeat. Stored 2 s after the last change.
- **Center**: opens the menu. In the menu, left / right change the selected value, center
  goes to the next entry, **hold center** leaves.
- Menu entries: brightness, dim after (idle backlight), module LED level, auto reboot
  (every 6 / 12 / 24 / 48 h of uptime, done between two tracks), version, uptime, heap,
  track count, sync state, reboot now, exit.
- When the backlight is dimmed, the first press only wakes it.

## Build and flash

PlatformIO with the **pioarduino** platform (Arduino core 3.3) pinned in `platformio.ini`.
It needs PlatformIO core ≥ 6.1.19 on Python ≤ 3.13; on dev37 that is
`~/.platformio/penv-py313/bin/pio`.

```sh
pio run -e cores3                       # build, copies bin/firmware-cores3.bin
pio run -e cores3 -t upload             # flash over USB-C
pio device monitor -b 115200            # log (health line every minute)
pio run -e fire -t upload               # M5Stack Fire
```

A CoreS3 that is not answering on its CDC port comes back by holding the reset button
until the LED blinks green (ROM download mode), then flashing again.

## Status

v0.1.0 — first firmware of the rewrite (2026-09-11). Builds for `cores3` and `fire`, not
yet run on hardware. Bench plan and the Nowde integration are in `docs/DESIGN.md`.
