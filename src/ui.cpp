#include "ui.h"

#include <M5Unified.h>

#include "config.h"
#include "fonts_orbitron.h"
#include "fonts_vt323.h"
#include "library.h"
#include "menu.h"

namespace {
M5Canvas g_cv(&M5.Display);
int g_bootY = 0;

// palette (RGB565)
constexpr uint16_t C_BG      = 0x0000;
constexpr uint16_t C_CYAN    = 0x07FF;
constexpr uint16_t C_CYAN_D  = 0x0410;   // dim cyan  (#008080)
constexpr uint16_t C_CYAN_DD = 0x0208;   // hairlines (#004040)
constexpr uint16_t C_YEL     = 0xFFE0;
constexpr uint16_t C_YEL_BG  = 0x2120;   // dark yellow fill (#202400)
constexpr uint16_t C_GRN     = 0x07E0;
constexpr uint16_t C_GRN_D   = 0x0320;   // dim green (#006400)
constexpr uint16_t C_RED     = 0xF800;

constexpr int W = 320, H = 240;
constexpr int TOP_H = 28, BOT_Y = 216;
constexpr int ROW_H = 26, ROWS = 7;               // 7 * 26 = 182
constexpr int LIST_Y = 32;                        // 32 .. 214
constexpr int CENTER_ROW = ROWS / 2;

const lgfx::GFXfont& F_HEAD  = hpfonts::Orbitron_13;
const lgfx::GFXfont& F_LABEL = hpfonts::Orbitron_11;
const lgfx::GFXfont& F_ROW   = hpfonts::VT323_18;
const lgfx::GFXfont& F_CUR   = hpfonts::VT323_26;
const lgfx::GFXfont& F_SMALL = hpfonts::VT323_16;
const lgfx::GFXfont& F_TIME  = hpfonts::VT323_22;

// marquee state for the playing row
int g_mqTrack = -1;
int g_mqOffset = 0;
int g_mqCycle = 0;          // text width + gap, 0 = fits, no scrolling
uint32_t g_mqPauseUntil = 0;
constexpr int MQ_GAP = 56;
constexpr int MQ_STEP = 3;  // px per frame at UI_PERIOD_MS

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

// file name without its extension
void baseTitle(const char* name, char* out, size_t n) {
  strlcpy(out, name, n);
  char* dot = strrchr(out, '.');
  if (dot && dot != out) *dot = 0;
}

// clip to a pixel width with "..." (never cut inside a UTF-8 sequence)
void clipTitle(char* s, int maxW) {
  if (g_cv.textWidth(s) <= maxW) return;
  size_t l = strlen(s);
  while (l > 1) {
    do { l--; } while (l > 0 && ((uint8_t)s[l] & 0xC0) == 0x80);
    s[l] = 0;
    if (g_cv.textWidth(s) + g_cv.textWidth("...") <= maxW) break;
  }
  strlcat(s, "...", cfg::MAX_NAME);
}

void drawTop(const PlayerSnapshot& s, const UiStatus& st) {
  g_cv.fillRect(0, 0, W, LIST_Y, C_BG);
  g_cv.setFont(&F_HEAD);
  g_cv.setTextDatum(middle_left);
  g_cv.setTextColor(C_CYAN, C_BG);
  g_cv.drawString("HPLAYER0", 8, TOP_H / 2);

  g_cv.setFont(&F_SMALL);
  g_cv.setTextDatum(middle_center);
  char b[32];
  if (!st.sdMounted) {
    g_cv.setTextColor(C_RED, C_BG);
    g_cv.drawString("NO SD CARD", W / 2 + 10, TOP_H / 2);
  } else if (st.trackCount == 0) {
    g_cv.setTextColor(C_RED, C_BG);
    g_cv.drawString("NO MEDIA", W / 2 + 10, TOP_H / 2);
  } else {
    snprintf(b, sizeof(b), "%u FILES", (unsigned)st.trackCount);
    g_cv.setTextColor(C_GRN, C_BG);
    g_cv.drawString(b, W / 2 + 10, TOP_H / 2);
  }
  if (st.syncLinked) {
    g_cv.setTextColor(C_GRN, C_BG);
    g_cv.drawString("SYNC", W / 2 + 70, TOP_H / 2);
  }

  g_cv.setFont(&F_TIME);
  g_cv.setTextDatum(middle_right);
  if (s.volume == 0) {
    g_cv.setTextColor(C_RED, C_BG);
    g_cv.drawString("MUTE", W - 8, TOP_H / 2);
  } else {
    snprintf(b, sizeof(b), "VOL %u", s.volume);
    g_cv.setTextColor(C_YEL, C_BG);
    g_cv.drawString(b, W - 8, TOP_H / 2);
  }
  g_cv.drawFastHLine(0, TOP_H, W, C_CYAN);
  g_cv.drawFastHLine(0, TOP_H + 2, W, C_CYAN_DD);
}

void drawButton(int cx, const char* label, uint16_t color) {
  int w = g_cv.textWidth(label) + 16;
  int x = cx - w / 2, y = BOT_Y + 4, h = H - y - 3;
  g_cv.drawRect(x, y, w, h, color);
  g_cv.setTextDatum(middle_center);
  g_cv.setTextColor(color, C_BG);
  g_cv.drawString(label, cx, y + h / 2 + 1);
}

void drawBottom(bool menuOpen) {
  g_cv.fillRect(0, BOT_Y, W, H - BOT_Y, C_BG);
  g_cv.drawFastHLine(0, BOT_Y, W, C_CYAN_DD);
  g_cv.setFont(&F_LABEL);
  if (menuOpen) {
    drawButton(53, "-", C_YEL);
    drawButton(160, "NEXT   HOLD=EXIT", C_YEL);
    drawButton(267, "+", C_YEL);
  } else {
    drawButton(53, "VOL -", C_CYAN);
    drawButton(160, "MENU", C_CYAN);
    drawButton(267, "VOL +", C_CYAN);
  }
}

void drawEmpty(const UiStatus& st) {
  g_cv.setFont(&F_CUR);
  g_cv.setTextDatum(middle_center);
  g_cv.setTextColor(C_RED, C_BG);
  g_cv.drawString(st.sdMounted ? "NO SUPPORTED MEDIA" : "INSERT A MICROSD CARD", W / 2, H / 2 - 14);
  g_cv.setFont(&F_ROW);
  g_cv.setTextColor(C_CYAN_D, C_BG);
  g_cv.drawString(st.sdMounted ? "mp3 wav aac m4a flac ogg opus" : "files are read from the card root", W / 2, H / 2 + 14);
}

void drawCurrentRow(const PlayerSnapshot& s, int idx, int y, uint32_t now) {
  g_cv.fillRect(2, y, W - 4, ROW_H, C_YEL_BG);
  g_cv.drawRect(2, y, W - 4, ROW_H, C_YEL);
  int cy = y + ROW_H / 2 - 2;

  // number + play mark
  char num[8];
  snprintf(num, sizeof(num), "%02d", idx + 1);
  g_cv.setFont(&F_CUR);
  g_cv.setTextDatum(middle_left);
  g_cv.setTextColor(C_YEL, C_YEL_BG);
  g_cv.drawString(num, 8, cy);
  int x = 8 + g_cv.textWidth(num) + 6;
  if (s.state == PlayerState::Playing) g_cv.fillTriangle(x, cy - 6, x, cy + 6, x + 9, cy, C_GRN);
  else g_cv.fillRect(x, cy - 5, 9, 10, C_RED);
  x += 16;

  // time or state, right aligned
  char right[40];
  uint16_t rightColor = C_GRN;
  if (s.state == PlayerState::Playing) {
    char t[16], d[16];
    fmtTime(t, sizeof(t), s.posSec);
    fmtTime(d, sizeof(d), s.durSec);
    snprintf(right, sizeof(right), "%s / %s", t, d);
  } else {
    strlcpy(right, player::stateName(s.state), sizeof(right));
    rightColor = C_RED;
  }
  g_cv.setFont(&F_TIME);
  g_cv.setTextDatum(middle_right);
  g_cv.setTextColor(rightColor, C_YEL_BG);
  g_cv.drawString(right, W - 8, cy);
  int rightW = g_cv.textWidth(right);

  // title: marquee when it does not fit
  char title[cfg::MAX_NAME];
  baseTitle(library::at(idx).name, title, sizeof(title));
  g_cv.setFont(&F_CUR);
  int maxW = (W - 8 - rightW - 10) - x;
  int tw = g_cv.textWidth(title);
  if (g_mqTrack != idx) {
    g_mqTrack = idx;
    g_mqOffset = 0;
    g_mqCycle = tw > maxW ? tw + MQ_GAP : 0;
    g_mqPauseUntil = now + 2000;
  }
  g_cv.setTextDatum(middle_left);
  g_cv.setTextColor(C_YEL, C_YEL_BG);
  if (g_mqCycle == 0) {
    g_cv.drawString(title, x, cy);
  } else {
    g_cv.setClipRect(x, y + 1, maxW, ROW_H - 2);
    g_cv.drawString(title, x - g_mqOffset, cy);
    g_cv.drawString(title, x - g_mqOffset + g_mqCycle, cy);
    g_cv.clearClipRect();
    if ((int32_t)(now - g_mqPauseUntil) >= 0) {
      g_mqOffset += MQ_STEP;
      if (g_mqOffset >= g_mqCycle) {
        g_mqOffset = 0;
        g_mqPauseUntil = now + 2000;
      }
    }
  }

  // progress bar inside the frame
  int bx = 8, bw = W - 16, by = y + ROW_H - 4;
  g_cv.fillRect(bx, by, bw, 2, C_GRN_D);
  if (s.durSec) {
    int p = (int)((uint64_t)bw * s.posSec / s.durSec);
    g_cv.fillRect(bx, by, p > bw ? bw : p, 2, C_GRN);
  }
}

void drawOtherRow(int idx, int y, int dist) {
  int cy = y + ROW_H / 2;
  uint16_t c = dist == 1 ? C_CYAN : C_CYAN_D;
  char num[8], title[cfg::MAX_NAME];
  snprintf(num, sizeof(num), "%02d", idx + 1);
  g_cv.setFont(&F_ROW);
  g_cv.setTextDatum(middle_left);
  g_cv.setTextColor(C_CYAN_D, C_BG);
  g_cv.drawString(num, 12, cy);
  baseTitle(library::at(idx).name, title, sizeof(title));
  clipTitle(title, W - 48 - 8);
  g_cv.setTextColor(c, C_BG);
  g_cv.drawString(title, 48, cy);
}

// which track sits on list row r for a given current track (-1 = empty row)
int trackAtRow(int r, int cur, int n) {
  int idx = cur + (r - CENTER_ROW);
  if (idx < 0 || idx >= n) {
    if (n >= ROWS) return ((idx % n) + n) % n;   // wrap only when the list fills the screen
    return -1;
  }
  return idx;
}

void drawList(const PlayerSnapshot& s, const UiStatus& st, uint32_t now) {
  g_cv.fillRect(0, LIST_Y, W, BOT_Y - LIST_Y, C_BG);
  int n = (int)st.trackCount;
  if (!st.sdMounted || n == 0) {
    drawEmpty(st);
    return;
  }
  int cur = s.track < 0 ? 0 : s.track;
  for (int r = 0; r < ROWS; ++r) {
    int idx = trackAtRow(r, cur, n);
    if (idx < 0) continue;
    int y = LIST_Y + r * ROW_H;
    if (r == CENTER_ROW) drawCurrentRow(s, idx, y, now);
    else drawOtherRow(idx, y, r < CENTER_ROW ? CENTER_ROW - r : r - CENTER_ROW);
  }
}

size_t menuFirst() {
  size_t sel = menu::selected();
  return sel >= (size_t)ROWS ? sel - ROWS + 1 : 0;
}

void drawMenu() {
  g_cv.fillRect(0, LIST_Y, W, BOT_Y - LIST_Y, C_BG);
  size_t n = menu::count();
  size_t sel = menu::selected();
  size_t first = menuFirst();
  for (size_t r = 0; r < (size_t)ROWS && first + r < n; ++r) {
    size_t i = first + r;
    int y = LIST_Y + r * ROW_H;
    int cy = y + ROW_H / 2;
    bool isSel = (i == sel);
    bool edit = menu::editable(i);
    uint16_t bg = isSel ? C_YEL_BG : C_BG;
    if (isSel) {
      g_cv.fillRect(2, y, W - 4, ROW_H, C_YEL_BG);
      g_cv.drawRect(2, y, W - 4, ROW_H, C_YEL);
    }
    char lab[24], val[40];
    menu::label(i, lab, sizeof(lab));
    menu::value(i, val, sizeof(val));
    g_cv.setFont(&F_ROW);
    g_cv.setTextDatum(middle_left);
    g_cv.setTextColor(isSel ? C_YEL : (edit ? C_CYAN : C_CYAN_D), bg);
    g_cv.drawString(lab, 12, cy);
    g_cv.setTextDatum(middle_right);
    g_cv.setTextColor(isSel ? C_GRN : (edit ? C_GRN_D : C_CYAN_D), bg);
    g_cv.drawString(val, W - 12, cy);
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
  M5.Display.setFont(&F_HEAD);
  M5.Display.setTextColor(C_CYAN, C_BG);
  M5.Display.setTextDatum(top_left);
  M5.Display.drawString("HPLAYER0", 8, g_bootY);
  M5.Display.drawFastHLine(0, g_bootY + 22, W, C_CYAN);
  g_bootY += 30;
}

void bootLine(const char* fmt, ...) {
  char buf[96];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  log_i("boot: %s", buf);
  if (g_bootY > H - 18) return;
  M5.Display.setFont(&F_ROW);
  M5.Display.setTextColor(C_GRN, C_BG);
  M5.Display.setTextDatum(top_left);
  M5.Display.drawString(buf, 8, g_bootY);
  g_bootY += 18;
}

void render(const PlayerSnapshot& s, const UiStatus& st) {
  uint32_t now = millis();
  drawTop(s, st);
  if (st.menuOpen) drawMenu();
  else drawList(s, st, now);
  drawBottom(st.menuOpen);
  push();
}

bool animating() { return g_mqCycle != 0; }

int trackAtY(int y, const PlayerSnapshot& s, size_t count) {
  if (y < LIST_Y || y >= BOT_Y || count == 0) return -1;
  int r = (y - LIST_Y) / ROW_H;
  if (r >= ROWS) return -1;
  return trackAtRow(r, s.track < 0 ? 0 : s.track, (int)count);
}

int menuItemAtY(int y) {
  if (y < LIST_Y || y >= BOT_Y) return -1;
  int r = (y - LIST_Y) / ROW_H;
  size_t i = menuFirst() + r;
  return i < menu::count() ? (int)i : -1;
}

}  // namespace ui
