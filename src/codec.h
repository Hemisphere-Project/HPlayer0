// Module Audio (M144) bring-up over M5.In_I2C: ES8388 codec (DAC only) and the STM32
// helper (jack detection, three RGB LEDs). I2S is NOT touched here — the audio engine
// owns it. Never open Wire on the Core's internal I2C pins: on a CoreS3 that re-muxes
// the touch controller's bus and the buttons stop working.
#pragma once
#include <Arduino.h>

struct AudioPins {
  int8_t bclk, lrck, dout, mclk;
};

namespace codec {
bool probe();                 // STM32 helper answers at 0x33
bool begin();                 // codec init, DAC -> headphone jack (OUT1), 48 kHz, mixers DAC-only
AudioPins pins();             // M-Bus I2S pins for this Core (A/B switch position implied)
void setVolume(uint8_t pct);  // 0..100 on the DAC
void setMute(bool mute);
bool present();

// STM32 helper
void setLedBrightness(uint8_t pct);      // 0..100
void setLeds(uint32_t rgb);              // all three
bool headphoneInserted();
}
