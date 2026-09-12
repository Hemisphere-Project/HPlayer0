// Bench console on the USB virtual serial port: one command per line.
//   help | info | next | prev | play N | stop | vol N | usb | eject | reboot
// Nothing here is needed on site; it exists so the bench can be driven from a laptop.
#pragma once
#include <Arduino.h>

namespace console {
void tick();
}
