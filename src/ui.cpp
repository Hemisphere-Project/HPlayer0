#include "ui.h"

#include <M5Unified.h>

#include "config.h"
#include "library.h"
#include "menu.h"

namespace {
M5Canvas g_cv(&M5.Display);
int g_bootY = 0;

constexpr uint16_t C_BG     = 0x0000;
constexpr uint16_t C_FG     = 0xE71C;   // light grey
constexpr uint16_t C_DIM    = 0x7BEF;   // mid grey
constexpr uint16_t C_ACC    = 0x05FF;   // cyan-ish accent
constexpr uint16_t C_ACC_BG = 0x0148;   // dark accent fill
constexpr uint16_t C_WARN   = 0xFD20;   // orange
constexpr uint16_t C_BAD    = 0xF800;   // red

constexpr int W = 320, H = 240;
constexpr int TOP_H = 24, BOT_H = 22;
constexpr int ROW_H = 26, ROWS = 7;               // 7 * 26 = 182
constexpr int LIST_Y = TOP_H + 6;                 // 30 .. 212
constexpr int CENTER_ROW = ROWS / 2;

void push() {
  SemaphoreHandle_t lock = player_busLock();
  if (lock) xSemaphoreTake(lock, portMAX_DELAY);
  g_cv.pushSprite(0, 0);
  if (lock) xSemaphoreGive(lock);
}

void fmtTime(char* out, size_t n, uint32_t sec) {
  if (sec >= 3600) snprintf(out, n, "%lu:%02lu:%02lu", (unsigned long)(sec / 3600), (unsigned long)((sec / 60) % 60), (unsigned long)(sec % 60));
  else snprintf(out, n, "%lu:%02lu", (unsigned long)(sec / 60), (unsigned long)(sec % 60));
}

// file name without its extension, clipped to a pixel width with an ellipsis
void trackTitle(const char* name, char* out, size_t n, int maxW) {
  strlcpy(out, name, n);
  char* dot = strrchr(out, '.');
  if (dot && dot != out) *dot = 0;
  while (out[0] && g_cv.textWidth(out) > maxW) {
    size_t l = strlen(out);
    if (l < 3) break;
    out[l - 1] = 0;
    // keep the ellipsis inside the budget
    if (g_cv.textWidth(out) + g_cv.textWidth("...") <= maxW) {
      strlcat(out, "...", n);
      break;
    }
  }
}

void drawTop(const PlayerSnapshot& s, const UiStatus& st) {
  g_cv.fillRect(0, 0, W, TOP_H, C_BG);
  g_cv.setFont(&fonts::DejaVu12);
  g_cv.setTextDatum(middle_left);
  g_cv.setTextColor(C_ACC, C_BG);
  g_cv.drawString("HPlayer0", 6, TOP_H / 2);

  g_cv.setTextDatum(middle_center);
  if (!st.sdMounted) {
    g_cv.setTextColor(C_BAD, C_BG);
    g_cv.drawString("NO SD", W / 2, TOP_H / 2);
  } else if (st.trackCount == 0) {
    g_cv.setTextColor(C_WARN, C_BG);
    g_cv.drawString("NO MEDIA", W / 2, TOP_H / 2);
  } else {
    char b[24];
    snprintf(b, sizeof(b), "%u files  sync %s", (unsigned)st.trackCount, st.syncState);
    g_cv.setTextColor(C_DIM, C_BG);
    g_cv.drawString(b, W / 2, TOP_H / 2);
  }

  char v[16];
  snprintf(v, sizeof(v), "VOL %u", s.volume);
  g_cv.setTextDatum(middle_right);
  g_cv.setTextColor(C_FG, C_BG);
  g_cv.drawString(v, W - 6, TOP_H / 2);
  g_cv.drawFastHLine(0, TOP_H - 1, W, C_ACC_BG);
}

void drawBottom(bool menuOpen) {
  int y = H - BOT_H;
  g_cv.fillRect(0, y, W, BOT_H, C_BG);
  g_cv.drawFastHLine(0, y, W, C_ACC_BG);
  g_cv.setFont(&fonts::DejaVu12);
  g_cv.setTextDatum(middle_center);
  g_cv.setTextColor(C_DIM, C_BG);
  int cy = y + BOT_H / 2 + 1;
  if (menuOpen) {
    g_cv.drawString("-", 53, cy);
    g_cv.drawString("NEXT  (hold: exit)", 160, cy);
    g_cv.drawString("+", 267, cy);
  } else {
    g_cv.drawString("VOL -", 53, cy);
    g_cv.drawString("MENU", 160, cy);
    g_cv.drawString("VOL +", 267, cy);
  }
}

void drawList(const PlayerSnapshot& s, const UiStatus& st) {
  g_cv.fillRect(0, TOP_H, W, H - TOP_H - BOT_H, C_BG);
  int n = (int)st.trackCount;
  if (!st.sdMounted || n == 0) {
    g_cv.setFont(&fonts::DejaVu18);
    g_cv.setTextDatum(middle_center);
    g_cv.setTextColor(st.sdMounted ? C_WARN : C_BAD, C_BG);
    g_cv.drawString(st.sdMounted ? "no supported media" : "insert a microSD card", W / 2, H / 2 - 10);
    g_cv.setFont(&fonts::DejaVu12);
    g_cv.setTextColor(C_DIM, C_BG);
    g_cv.drawString(st.sdMounted ? "mp3 wav aac m4a flac ogg opus" : "files are read from the card root", W / 2, H / 2 + 16);
    return;
  }
  int cur = s.track < 0 ? 0 : s.track;
  for (int r = 0; r < ROWS; ++r) {
    int off = r - CENTER_ROW;
    int idx = cur + off;
    // wrap only when the list is long enough to make wrapping meaningful
    if (idx < 0 || idx >= n) {
      if (n >= ROWS) idx = ((idx % n) + n) % n;
      else continue;
    }
    int y = LIST_Y + r * ROW_H;
    bool isCur = (off == 0);
    char title[cfg::MAX_NAME];
    if (isCur) {
      g_cv.fillRoundRect(2, y, W - 4, ROW_H, 4, C_ACC_BG);
      g_cv.setFont(&fonts::DejaVu18);
      g_cv.setTextColor(C_FG, C_ACC_BG);
      char t[24], d[16];
      fmtTime(t, sizeof(t), s.posSec);
      fmtTime(d, sizeof(d), s.durSec);
      char times[40];
      if (s.state == PlayerState::Playing) snprintf(times, sizeof(times), "%s / %s", t, d);
      else strlcpy(times, player::stateName(s.state), sizeof(times));
      g_cv.setFont(&fonts::DejaVu12);
      int tw = g_cv.textWidth(times);
      g_cv.setTextDatum(middle_right);
      g_cv.setTextColor(C_ACC, C_ACC_BG);
      g_cv.drawString(times, W - 8, y + ROW_H / 2 - 2);
      g_cv.setFont(&fonts::DejaVu18);
      trackTitle(library::at(idx).name, title, sizeof(title), W - 20 - tw - 10);
      g_cv.setTextDatum(middle_left);
      g_cv.setTextColor(C_FG, C_ACC_BG);
      g_cv.drawString(title, 8, y + ROW_H / 2 - 2);
      // progress bar along the bottom edge of the row
      int bw = W - 16;
      g_cv.fillRect(8, y + ROW_H - 4, bw, 2, C_DIM);
      if (s.durSec) {
        int p = (int)((uint64_t)bw * s.posSec / s.durSec);
        g_cv.fillRect(8, y + ROW_H - 4, p > bw ? bw : p, 2, C_ACC);
      }
    } else {
      int dist = off < 0 ? -off : off;
      g_cv.setFont(&fonts::DejaVu12);
      g_cv.setTextColor(dist == 1 ? C_FG : C_DIM, C_BG);
      g_cv.setTextDatum(middle_left);
      char num[8];
      snprintf(num, sizeof(num), "%d", idx + 1);
      g_cv.drawString(num, 12, y + ROW_H / 2);
      trackTitle(library::at(idx).name, title, sizeof(title), W - 48);
      g_cv.drawString(title, 40, y + ROW_H / 2);
    }
  }
}

void drawMenu() {
  g_cv.fillRect(0, TOP_H, W, H - TOP_H - BOT_H, C_BG);
  size_t n = menu::count();
  size_t sel = menu::selected();
  // keep the selection visible: scroll window of ROWS entries
  size_t first = 0;
  if (sel >= (size_t)ROWS) first = sel - ROWS + 1;
  g_cv.setFont(&fonts::DejaVu12);
  for (size_t r = 0; r < (size_t)ROWS && first + r < n; ++r) {
    size_t i = first + r;
    int y = LIST_Y + r * ROW_H;
    bool isSel = (i == sel);
    uint16_t bg = isSel ? C_ACC_BG : C_BG;
    if (isSel) g_cv.fillRoundRect(2, y, W - 4, ROW_H, 4, bg);
    char lab[24], val[40];
    menu::label(i, lab, sizeof(lab));
    menu::value(i, val, sizeof(val));
    g_cv.setTextDatum(middle_left);
    g_cv.setTextColor(isSel ? C_FG : (menu::editable(i) ? C_FG : C_DIM), bg);
    g_cv.drawString(lab, 12, y + ROW_H / 2);
    g_cv.setTextDatum(middle_right);
    g_cv.setTextColor(isSel ? C_ACC : C_DIM, bg);
    g_cv.drawString(val, W - 12, y + ROW_H / 2);
  }
}
}  // namespace

namespace ui {

void begin(uint8_t brightness) {
  M5.Display.setRotation(1);
  M5.Display.setBrightness(brightness);
  M5.Display.fillScreen(C_BG);
  g_cv.setPsram(true);
  g_cv.setColorDepth(16);
  if (!g_cv.createSprite(W, H)) log_e("sprite alloc failed");
  bootScreen();
}

void bootScreen() {
  g_bootY = 8;
  M5.Display.fillScreen(C_BG);
  M5.Display.setFont(&fonts::DejaVu18);
  M5.Display.setTextColor(C_ACC, C_BG);
  M5.Display.setTextDatum(top_left);
  M5.Display.drawString("HPlayer0", 8, g_bootY);
  g_bootY += 28;
  M5.Display.setFont(&fonts::DejaVu12);
  M5.Display.setTextColor(C_FG, C_BG);
}

void bootLine(const char* fmt, ...) {
  char buf[96];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  log_i("boot: %s", buf);
  if (g_bootY > H - 16) return;
  M5.Display.setFont(&fonts::DejaVu12);
  M5.Display.setTextColor(C_FG, C_BG);
  M5.Display.setTextDatum(top_left);
  M5.Display.drawString(buf, 8, g_bootY);
  g_bootY += 16;
}

void render(const PlayerSnapshot& s, const UiStatus& st) {
  drawTop(s, st);
  if (st.menuOpen) drawMenu();
  else drawList(s, st);
  drawBottom(st.menuOpen);
  push();
}

}  // namespace ui
