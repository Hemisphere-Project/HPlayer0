/*
    HPlayer0 — looping audio player for M5Stack Core + Module Audio (M144).
    Copyright (C) 2024-2026 Thomas BOHL - thomas@37m.gr

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <Arduino.h>
#include <M5Unified.h>
#include <esp_system.h>

#include "codec.h"
#include "console.h"
#include "config.h"
#include "library.h"
#include "menu.h"
#include "player.h"
#include "settings.h"
#include "supervisor.h"
#include "sync.h"
#include "ui.h"
#include "usbdrive.h"

namespace {
Settings g_settings;
uint32_t g_lastRender = 0, g_lastGen = 0, g_lastPos = 0, g_lastRepeat = 0;
bool g_lastMenu = false;
bool g_swallowTap = false;
uint32_t g_lastView = 0, g_lastUsbGen = 0;

const char* resetReason() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:  return "power-on";
    case ESP_RST_SW:       return "software";
    case ESP_RST_PANIC:    return "PANIC";
    case ESP_RST_INT_WDT:  return "INT-WDT";
    case ESP_RST_TASK_WDT: return "TASK-WDT";
    case ESP_RST_WDT:      return "WDT";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_USB:      return "usb";
    case ESP_RST_JTAG:     return "jtag";
    default:               return "other";
  }
}

void volumeStep(int dir) {
  int v = (int)g_settings.volume + dir * (int)cfg::VOL_STEP;
  v = v < 0 ? 0 : (v > 100 ? 100 : v);
  if (v == g_settings.volume) return;
  g_settings.volume = (uint8_t)v;
  player::setVolume(g_settings.volume);
  settings::markDirty();
}

// short press = one step; held = auto-repeat
bool stepPressed(m5::Button_Class& b, uint32_t now) {
  if (b.wasClicked()) return true;
  if (b.pressedFor(cfg::BTN_REPEAT_DELAY) && now - g_lastRepeat >= cfg::BTN_REPEAT_MS) {
    g_lastRepeat = now;
    return true;
  }
  return false;
}

void handleInput(uint32_t now) {
  auto& t = M5.Touch.getDetail();
  bool touchOnScreen = t.wasPressed() && t.y < 240;   // the strip below the LCD is BtnA/B/C
  bool pressed = M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.BtnC.wasPressed() || touchOnScreen;
  if (pressed && supervisor::noteInput()) {
    // the press only woke the backlight — swallow it (release events too)
    M5.BtnA.setRawState(now, false);
    M5.BtnB.setRawState(now, false);
    M5.BtnC.setRawState(now, false);
    g_swallowTap = true;
    return;
  }
  // USB drive: the offer modal takes every input, the drive screen only the forced exit
  if (usbdrive::state() == usbdrive::State::Offered) {
    if (M5.BtnA.wasClicked()) usbdrive::answer(false);
    if (M5.BtnC.wasClicked()) usbdrive::answer(true);
    if (t.wasClicked() && t.y < 240) {
      if (g_swallowTap) g_swallowTap = false;
      else {
        int h = ui::modalHit(t.x, t.y);
        if (h >= 0) usbdrive::answer(h == 1);
      }
    }
    return;
  }
  if (usbdrive::state() == usbdrive::State::Active) {
    if (M5.BtnB.wasHold()) usbdrive::leave();
    if (t.wasClicked()) g_swallowTap = false;
    return;
  }
  if (!menu::isOpen()) {
    if (t.isPressed() && t.y < 240) ui::dragBy(t.deltaY(), player::snapshot(), library::count(), now);
    if (t.wasReleased()) ui::dragEnd(player::snapshot(), library::count(), now);
  }
  if (t.wasClicked() && t.y < 240) {
    if (g_swallowTap) g_swallowTap = false;
    else if (menu::isOpen()) {
      int i = ui::menuItemAtY(t.y);
      if (i >= 0) {
        if ((size_t)i == menu::selected()) menu::activate();
        else menu::select(i);
      }
    } else {
      int idx = ui::trackAtY(t.y, player::snapshot(), library::count());
      if (idx >= 0) player::play(idx);
    }
  }
  if (menu::isOpen()) {
    if (M5.BtnB.wasHold()) menu::close();
    else if (M5.BtnB.wasClicked()) menu::onCenter();
    if (stepPressed(M5.BtnA, now)) menu::onLeft();
    if (stepPressed(M5.BtnC, now)) menu::onRight();
  } else {
    if (M5.BtnB.wasClicked()) menu::open();
    if (stepPressed(M5.BtnA, now)) volumeStep(-1);
    if (stepPressed(M5.BtnC, now)) volumeStep(+1);
  }
}
}  // namespace

void console_setVolume(uint8_t pct) {
  g_settings.volume = pct;
  player::setVolume(pct);
  settings::markDirty();
}

void setup() {
  auto cfgm5 = M5.config();
  cfgm5.internal_spk = false;   // the Module Audio owns I2S; keep M5.Speaker off the bus
  cfgm5.internal_mic = false;   // and the CoreS3's ES7210 off the shared pins
  cfgm5.clear_display = true;
  M5.begin(cfgm5);
  M5.BtnB.setHoldThresh(cfg::MENU_HOLD_MS);

  settings::load(g_settings);
  ui::begin(g_settings.brightness);
  ui::bootLine("fw %s  board %s", HP_VERSION, HP_BOARD);
  ui::bootLine("reset: %s", resetReason());
  log_i("HPlayer0 %s on %s (board id %d), reset %s", HP_VERSION, HP_BOARD, (int)M5.getBoard(), resetReason());

  bool mod = codec::probe();
  ui::bootLine("module audio: %s", mod ? "found" : "NOT FOUND");
  if (mod) {
    if (!codec::begin()) ui::bootLine("codec init FAILED");
  }
  AudioPins pins = codec::pins();
  ui::bootLine("i2s bclk=%d lrck=%d dout=%d mclk=%d", pins.bclk, pins.lrck, pins.dout, pins.mclk);

  bool sd = library::mount();
  ui::bootLine("sd card: %s", sd ? "ok" : "none");
  size_t n = sd ? library::scan() : 0;
  if (sd) ui::bootLine("%u tracks", (unsigned)n);

  usbdrive::begin();   // needs the card's sector count; the log moves to the USB serial from here
  log_i("HPlayer0 %s on %s (board id %d), reset %s, %u tracks", HP_VERSION, HP_BOARD, (int)M5.getBoard(),
        resetReason(), (unsigned)n);
  ui::bootLine("usb: serial + drive ready");

  player::begin(pins, g_settings.volume);
  codec::setVolume(g_settings.volume);
  syncgrp::begin();
  menu::begin(&g_settings);
  supervisor::begin(&g_settings);
  player::onLibraryChanged();
  delay(400);
}

void loop() {
  uint32_t now = millis();
  M5.update();
  handleInput(now);
  player::tick();
  usbdrive::tick(now);
  console::tick();
  ui::tick(player::snapshot(), library::count(), now);
  supervisor::tick(menu::isOpen());
  settings::tick(g_settings);

  uint32_t period = ui::renderPeriod();
  if (period == 0) period = cfg::UI_PERIOD_MS;
  if (now - g_lastRender >= period) {
    PlayerSnapshot s = player::snapshot();
    bool menuOpen = menu::isOpen();
    bool changed = s.generation != g_lastGen || s.posSec != g_lastPos || menuOpen != g_lastMenu ||
                   ui::viewGeneration() != g_lastView || usbdrive::generation() != g_lastUsbGen ||
                   menuOpen /* live values */ || usbdrive::state() == usbdrive::State::Offered ||
                   ui::animating() || now - g_lastRender >= 1000;
    if (changed) {
      UiStatus st;
      st.sdMounted = library::mounted();
      st.trackCount = library::count();
      st.syncState = syncgrp::stateName();
      st.syncLinked = syncgrp::linked();
      st.menuOpen = menuOpen;
      st.usbState = (uint8_t)usbdrive::state();
      st.offerMs = usbdrive::offerRemainingMs(now);
      st.usbRead = usbdrive::bytesRead();
      st.usbWritten = usbdrive::bytesWritten();
      ui::render(s, st);
      g_lastGen = s.generation;
      g_lastUsbGen = usbdrive::generation();
      g_lastView = ui::viewGeneration();
      g_lastPos = s.posSec;
      g_lastMenu = menuOpen;
      g_lastRender = now;
    }
  }
  vTaskDelay(pdMS_TO_TICKS(5));
}
