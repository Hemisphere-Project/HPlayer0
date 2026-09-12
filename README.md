# HPlayer0

**Looping audio player for M5Stack Core + Module Audio (M144), from a microSD card.**

Drop audio files on a card, plug the card in, power the box: it plays every supported file
in order, forever. The screen shows the media list centred on the playing track with its
position and duration; the three buttons are **volume -**, **menu**, **volume +**. Built for
exhibitions that run every day for months: read-only card, watchdogs, stall detection,
SD hot-plug, optional scheduled reboot at the end of the playlist.

Hemisphere Project · GPL-3.0 · repo `github.com/Hemisphere-Project/HPlayer0`.
The 2024 prototype (WAV + LoRa + USB-MIDI + Bluetooth) is parked on branch `proto-2024`.

## Hardware

| Core | Env | Module Audio pin switch | Buttons |
|------|-----|-------------------------|---------|
| M5Stack **CoreS3 SE** (and CoreS3) | `cores3` | **B** | touch strip under the screen, and the screen itself |

The CoreS3 SE is the only target. The I2S pins come from M5Unified's M-Bus table, so a Fire
(switch A) may work with an `m5stack-fire` env, but nothing is built or tested for it. The
Core Basic has no PSRAM, which this engine needs (`docs/DESIGN.md`).

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

## Screen, buttons and menu

- The list is centred on the playing track and never wraps: rows beyond the first and the
  last file stay blank. **Drag the list** to browse (it stops at the ends), it recentres six
  seconds after the last touch or when the track changes. The playing
  row shows its number, a play mark, the title over the full width (scrolling when it does
  not fit), a progress bar and a time chip on the frame's top-right corner. Accented
  titles render (Latin-1). **Tap a row to play that track.**
- **Left / right**: volume in steps of 5, hold to repeat (default 80). Stored 2 s after the
  last change. Volume 0 mutes the DAC.
- **Center**: opens the menu. The menu is the same wheel as the list: the selected entry
  sits in the centre, drag to move it, tap an entry to select it and again to act. Left /
  right change the selected value, center (NEXT) steps to the next entry, **hold center**
  leaves.
- Menu entries: brightness, dim after (idle backlight), module LED level, auto reboot
  (every 6 / 12 / 24 / 48 h of uptime, taken at the end of the playlist so playback restarts
  from the first media), USB drive mode, version, uptime, heap, track count, sync state,
  reboot now, exit.
- When idle, the backlight dims to 50 % of the set brightness; any press, touch or drag
  only wakes it. Motion wake is coded for Cores with an IMU, but the CoreS3 SE has none
  (M5Unified finds no IMU on it), so on the SE only a touch or a button wakes the screen.

## USB drive mode

Plug the box into a computer and it shows up as a removable disk called **HPlayer0** with
no card inside, plus a serial port. The box keeps playing and asks on screen whether to
switch to **USB drive mode** (YES / NO, the question closes by itself after 15 s; the menu
has the same entry). Say yes and the microSD appears on the computer as a plain FAT drive:
add, rename or delete files, then **eject** it. The box takes the card back, rescans and
restarts from the first file. A phone charger never triggers the question: only a real
host that enumerates the device does.

Only one side can hold the card, which is why playback stops in drive mode. Transfers run
at USB full speed over the card's SPI bus, about 500 kB/s each way, so a 40 MB file takes
about 80 s. If the computer will not eject, hold MENU on the box to force the card back
(after making sure the copy is finished).

## Bench console

The serial port (115200 baud) carries the log and takes one command per line:
`help`, `info`, `bench`, `dump`, `next`, `prev`, `play N`, `stop`, `vol N`, `usb`, `eject`,
`reboot`. The log is muted while the computer holds the card; `dump` prints what was logged
meanwhile. Open the port once: a second opener drops DTR and silences the output.

## Build and flash

PlatformIO with the **pioarduino** platform (Arduino core 3.3) pinned in `platformio.ini`.
It needs PlatformIO core ≥ 6.1.19 on Python ≤ 3.13; on dev37 that is
`~/.platformio/penv-py313/bin/pio`.

```sh
pio run -e cores3                       # build, copies bin/firmware-cores3.bin
pio run -e cores3 -t upload             # flash through the box's own serial port
pio device monitor -b 115200            # log (health line every minute)
```

The firmware runs the native USB stack (TinyUSB, serial + disk), so flashing goes through
its virtual serial port: PlatformIO opens it at 1200 baud, the firmware drops into the ROM
bootloader, esptool flashes and resets. A box whose firmware never brought USB up (a bad
build) is flashed the ROM way: hold the reset button until the LED blinks green, plug in,
flash.

Fonts: Share Tech, Share Tech Mono and Orbitron (SIL OFL, `include/OFL-*.txt`) embedded as
1-bit GFX fonts with Latin-1 coverage, generated by `scripts/gen_gfxfont.py` (`uv run --with pillow`).

## Status

v0.1.0 — first firmware of the rewrite (2026-09-11), running on the bench CoreS3 SE: all
six formats decode, transitions are clean, heap flat. Bench plan and the Nowde integration
are in `docs/DESIGN.md`.
