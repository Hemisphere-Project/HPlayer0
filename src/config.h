// HPlayer0 — compile-time constants. Runtime settings live in settings.h (NVS).
#pragma once
#include <Arduino.h>

#ifndef HP_VERSION
#define HP_VERSION "dev"
#endif
#ifndef HP_BOARD
#define HP_BOARD "unknown"
#endif

namespace cfg {

// Module Audio (M144): STM32 helper at 0x33 (jack detect + RGB LEDs), ES8388 codec at 0x10.
// Probe 0x33 only — on a CoreS3 the internal BMM150 magnetometer answers at 0x10.
constexpr uint8_t  I2C_MODAUDIO   = 0x33;
constexpr uint32_t I2C_FREQ       = 400000;

// microSD on the Core's SPI bus, CS = GPIO4 on Basic / Fire / Core2 / CoreS3.
constexpr int      SD_CS          = 4;
constexpr uint32_t SD_SPI_HZ      = 25000000;

constexpr size_t   MAX_FILES      = 256;
constexpr size_t   MAX_NAME       = 96;

constexpr uint8_t  VOL_DEFAULT    = 80;    // codec DAC volume, percent
constexpr uint8_t  VOL_STEP       = 5;
constexpr uint8_t  BRIGHT_DEFAULT = 160;   // 0..255
constexpr uint8_t  BRIGHT_MIN     = 10;
constexpr uint8_t  DIM_PERCENT    = 50;    // idle backlight, as a share of the set brightness
constexpr float    WAKE_G         = 0.15f; // accel change between two IMU samples that wakes the screen

constexpr uint32_t UI_PERIOD_MS       = 100;
constexpr uint32_t BTN_REPEAT_DELAY   = 400;
constexpr uint32_t BTN_REPEAT_MS      = 120;
constexpr uint32_t MENU_HOLD_MS       = 700;

constexpr uint32_t SD_RETRY_MS        = 2000;   // remount attempts while no card
constexpr uint32_t OPEN_FAIL_SWEEP_MS = 5000;   // all tracks failed: retry after
constexpr uint32_t STALL_MS           = 15000;  // playing but position frozen -> restart track
constexpr uint32_t STALL_REBOOT_MS    = 45000;  // still frozen -> reboot
constexpr uint32_t PUMP_WDT_S         = 30;     // task watchdog on the audio pump
constexpr uint32_t HEALTH_LOG_MS      = 60000;
constexpr uint32_t NVS_SAVE_DELAY_MS  = 2000;   // coalesce setting writes

}  // namespace cfg
