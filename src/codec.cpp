#include "codec.h"

#include <M5Unified.h>
#include <es8388.hpp>

#include "config.h"

namespace {
ES8388* g_es = nullptr;
bool g_present = false;

// STM32 helper registers (M5Module-Audio README)
constexpr uint8_t REG_HP_INSERT   = 0x20;
constexpr uint8_t REG_LED_BRIGHT  = 0x30;
constexpr uint8_t REG_LED         = 0x40;   // 9 bytes RGB x3
constexpr uint8_t REG_MIC_STATUS  = 0x00;
constexpr uint8_t REG_HPMIC       = 0x11;

void stmWrite(uint8_t reg, const uint8_t* buf, size_t len) {
  if (!g_present) return;
  if (!M5.In_I2C.writeRegister(cfg::I2C_MODAUDIO, reg, buf, len, cfg::I2C_FREQ)) {
    log_w("modaudio stm32 write 0x%02x failed", reg);
  }
}
}  // namespace

namespace codec {

bool probe() {
  g_present = M5.In_I2C.scanID(cfg::I2C_MODAUDIO);
  return g_present;
}

bool present() { return g_present; }

AudioPins pins() {
  // Same table the M5 wrapper uses. CoreS3: switch B (BCLK on bus pin 24, MCLK on 22).
  // Basic / Fire / Core2: switch A (BCLK on 22, MCLK on 24).
  AudioPins p;
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  p.bclk = M5.getPin(m5::pin_name_t::mbus_pin24);
  p.mclk = M5.getPin(m5::pin_name_t::mbus_pin22);
#else
  p.bclk = M5.getPin(m5::pin_name_t::mbus_pin22);
  p.mclk = M5.getPin(m5::pin_name_t::mbus_pin24);
#endif
  p.lrck = M5.getPin(m5::pin_name_t::mbus_pin21);
  p.dout = M5.getPin(m5::pin_name_t::mbus_pin23);
  return p;
}

bool begin() {
  if (!g_present) return false;
  if (!g_es) g_es = new ES8388(&M5.In_I2C, cfg::I2C_FREQ);
  if (!g_es->init()) {
    log_e("es8388 init failed");
    return false;
  }
  // The stock init leaves a mic monitor path open (micbias on, PGA 24 dB, mixers with the
  // line amp mixed in): on a TRRS headset that is a constant hiss. This player never
  // records — DAC-only mixers, whole ADC path down, headphone output only.
  g_es->setMixSourceSelect(MIXRES, MIXRES);  // ignored by some revisions; the raw writes below win
  M5.In_I2C.writeRegister8(ES8388_ADDR, ES8388_DACCONTROL17, 0x90, cfg::I2C_FREQ);
  M5.In_I2C.writeRegister8(ES8388_ADDR, ES8388_DACCONTROL20, 0x90, cfg::I2C_FREQ);
  M5.In_I2C.writeRegister8(ES8388_ADDR, ES8388_ADCPOWER, 0xFF, cfg::I2C_FREQ);
  g_es->setDACOutput(DAC_OUTPUT_OUT1);      // TRRS headphone jack
  g_es->setBitsSample(ES_MODULE_DAC, BIT_LENGTH_16BITS);
  g_es->setSampleRate(SAMPLE_RATE_48K);     // matches the engine's pinned output clock
  g_es->setDACVolume(0);                    // player sets the real level once it starts
  g_es->setDACmute(false);

  uint8_t v = 0;
  stmWrite(REG_MIC_STATUS, &v, 1);          // mic/line input off
  stmWrite(REG_HPMIC, &v, 1);               // headset mic off
  log_i("es8388 ready");
  return true;
}

void setVolume(uint8_t pct) {
  if (!g_es) return;
  if (pct > 100) pct = 100;
  // the attenuator bottoms out at -45 dB, not silence: 0 means mute
  g_es->setDACVolume(pct);
  g_es->setDACmute(pct == 0);
}

void setMute(bool mute) {
  if (g_es) g_es->setDACmute(mute);
}

void setLedBrightness(uint8_t pct) {
  if (pct > 100) pct = 100;
  stmWrite(REG_LED_BRIGHT, &pct, 1);
}

void setLeds(uint32_t rgb) {
  uint8_t buf[9];
  for (int i = 0; i < 3; ++i) {
    buf[i * 3 + 0] = (rgb >> 16) & 0xff;
    buf[i * 3 + 1] = (rgb >> 8) & 0xff;
    buf[i * 3 + 2] = rgb & 0xff;
  }
  stmWrite(REG_LED, buf, sizeof(buf));
}

bool headphoneInserted() {
  if (!g_present) return false;
  uint8_t v = 0;
  if (!M5.In_I2C.readRegister(cfg::I2C_MODAUDIO, REG_HP_INSERT, &v, 1, cfg::I2C_FREQ)) return false;
  return v != 0;
}

}  // namespace codec
