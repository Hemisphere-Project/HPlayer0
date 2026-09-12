// USB drive mode: the CoreS3 SE's native USB port presents the microSD to a computer as a
// removable disk (TinyUSB mass storage, sector callbacks on the SD card). Only one side can
// own the FAT filesystem, so playback stops while the computer holds the card, and the
// player remounts, rescans and restarts from the first media when the computer ejects it.
//
// A computer that enumerates the box (a charger does not) triggers an offer on screen;
// playback continues meanwhile and the offer times out on its own.
#pragma once
#include <Arduino.h>

namespace usbdrive {
enum class State : uint8_t { Off, Offered, Active };

void begin();                        // descriptors, CDC log, MSC (media absent), USB.begin
void tick(uint32_t now);             // state transitions, UI task
State state();
bool hostConnected();                // a computer has enumerated the device
void answer(bool yes);               // the offer's YES / NO
void enter();                        // switch to drive mode (menu, offer, console)
void leave();                        // back to the player (eject, unplug, forced)
uint32_t offerRemainingMs(uint32_t now);
uint32_t bytesRead();
uint32_t bytesWritten();
uint32_t generation();               // bumps when something the screen shows changed
uint32_t ioErrors();                 // failed sector callbacks
void bench(Print& out);              // local SD read throughput, multi-block vs single-sector
}
