#include "ui.h"

#include <M5Unified.h>

#include <math.h>

#include "config.h"
#include "fonts_orbitron.h"
#include "fonts_sharetech.h"
#include "fonts_sharetechmono.h"
#include "library.h"
#include "menu.h"

namespace {
M5Canvas g_cv(&M5.Display);
int g_bootY = 0;

// palette (RGB565)
constexpr uint16_t C_BG      = 0x0000;
constexpr uint16_t C_CYAN    = 0x07FF;
constexpr uint16_t C_CYAN_M  = 0x0514;   // mid cyan  (#00a0a0) — the two neighbours
constexpr uint16_t C_CYAN_D  = 0x030C;   // dim cyan  (#006060) — the rest of the list
constexpr uint16_t C_CYAN_DD = 0x0208;   // hairlines (#004040)
constexpr uint16_t C_YEL     = 0xFFE0;
constexpr uint16_t C_YEL_BG  = 0x18E0;   // faint yellow fill (#181c00)
constexpr uint16_t C_GRN     = 0x07E0;
constexpr uint16_t C_GRN_D   = 0x0320;   // dim green (#006400)
constexpr uint16_t C_TRACK   = 0x2945;   // progress bar track, neutral dark grey (#2c2c2c)
constexpr uint16_t C_RED     = 0xF800;
constexpr uint16_t C_FG_DIM  = 0x9CD3;   // light grey text (#999999)

constexpr int W = 320, H = 240;
constexpr int TOP_H = 26, BOT_Y = 220;
constexpr int ROWS = 7, CENTER_ROW = ROWS / 2;
constexpr int ROW_H = 26;                         // the six neighbours
constexpr int CUR_H = 34;                         // the playing row: title, gap, progress bar
constexpr int LIST_Y = 30;                        // 30 + 6*26 + 34 = 220 = BOT_Y
constexpr int PAD_X = 12;
constexpr int FRAME_X = 4;                        // playing-row frame inset

// browse (touch scrolling) state: the window is centred on the playing track plus a row
// offset, and the whole list carries a pixel shift that follows the finger and eases back
// to 0 (row snap on release, recentre after a few seconds idle)
int g_browse = 0;            // rows
float g_shift = 0;           // px, positive = content drawn lower
bool g_anim = false;         // easing g_shift back to 0
bool g_touching = false;
uint32_t g_lastTouch = 0;
uint32_t g_viewGen = 1;
int g_lastTrack = -1;
int g_playRow = CENTER_ROW;  // window row holding the playing track (may be outside 0..ROWS-1)
constexpr uint32_t RECENTRE_MS = 6000;

// row r of the window: 0..ROWS-1 visible, others drawn only while shifted
int rowY(int r, int playRow) { return LIST_Y + r * ROW_H + (r > playRow ? CUR_H - ROW_H : 0); }
int rowH(int r, int playRow) { return r == playRow ? CUR_H : ROW_H; }

const lgfx::GFXfont& F_HEAD  = hpfonts::Orbitron_13;
const lgfx::GFXfont& F_LABEL = hpfonts::ShareTech_14;
const lgfx::GFXfont& F_ROW   = hpfonts::ShareTech_18;
const lgfx::GFXfont& F_CUR   = hpfonts::ShareTech_22;
const lgfx::GFXfont& F_SMALL = hpfonts::ShareTech_16;
const lgfx::GFXfont& F_TIME  = hpfonts::ShareTechMono_15;
const lgfx::GFXfont& F_VOL   = hpfonts::ShareTechMono_18;

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

// "HPLAYER·0": Orbitron has no middle dot glyph, so the dot is drawn by hand
template <typename GFX>
int brandWidth(GFX& g) {
  g.setFont(&F_HEAD);
  return g.textWidth("HPLAYER") + 5 + 3 + 8 + g.textWidth("0");
}

template <typename GFX>
void drawBrand(GFX& g, int x, int cy) {
  g.setFont(&F_HEAD);
  g.setTextDatum(middle_left);
  g.setTextColor(C_CYAN, C_BG);
  g.drawString("HPLAYER", x, cy);
  x += g.textWidth("HPLAYER") + 5;
  g.fillRect(x, cy - 1, 3, 3, C_CYAN);
  x += 8;
  g.drawString("0", x, cy);
}

void drawTop(const PlayerSnapshot& s, const UiStatus& st) {
  g_cv.fillRect(0, 0, W, LIST_Y, C_BG);
  drawBrand(g_cv, 8, TOP_H / 2);
  int leftEnd = 8 + brandWidth(g_cv);

  char b[40];
  g_cv.setFont(&F_VOL);
  g_cv.setTextDatum(middle_right);
  if (s.volume == 0) {
    strlcpy(b, "MUTE", sizeof(b));
    g_cv.setTextColor(C_RED, C_BG);
  } else {
    snprintf(b, sizeof(b), "VOL %u", s.volume);
    g_cv.setTextColor(C_YEL, C_BG);
  }
  g_cv.drawString(b, W - 8, TOP_H / 2);
  int rightStart = W - 8 - g_cv.textWidth(b);
  if (st.syncLinked) {
    g_cv.fillCircle(rightStart - 10, TOP_H / 2, 3, C_GRN);
    rightStart -= 16;
  }

  // status, small, centred in the free span between the brand and the volume
  int cx = (leftEnd + rightStart) / 2;
  g_cv.setTextDatum(middle_center);
  g_cv.setFont(&F_TIME);
  if (!st.sdMounted) {
    g_cv.setTextColor(C_RED, C_BG);
    g_cv.drawString("NO SD CARD", cx, TOP_H / 2 + 1);
  } else if (st.trackCount == 0) {
    g_cv.setTextColor(C_RED, C_BG);
    g_cv.drawString("NO MEDIA", cx, TOP_H / 2 + 1);
  } else if (st.usbState == 2) {
    g_cv.setTextColor(C_CYAN, C_BG);
    g_cv.drawString("USB DRIVE", cx, TOP_H / 2 + 1);
  } else if (s.state == PlayerState::Playing) {
    char t[16], d[16];
    fmtTime(t, sizeof(t), s.posSec);
    fmtTime(d, sizeof(d), s.durSec);
    snprintf(b, sizeof(b), "%s / %s", t, d);
    g_cv.setTextColor(C_GRN, C_BG);
    g_cv.drawString(b, cx, TOP_H / 2 + 1);
  } else {
    g_cv.setTextColor(C_RED, C_BG);
    g_cv.drawString(player::stateName(s.state), cx, TOP_H / 2 + 1);
  }
  g_cv.drawFastHLine(0, TOP_H, W, C_CYAN_DD);
}

void drawButton(int cx, const char* label, uint16_t color) {
  g_cv.setTextDatum(middle_center);
  g_cv.setTextColor(color, C_BG);
  g_cv.drawString(label, cx, BOT_Y + (H - BOT_Y) / 2 + 1);
}

void drawBottom(bool menuOpen) {
  g_cv.fillRect(0, BOT_Y, W, H - BOT_Y, C_BG);
  g_cv.drawFastHLine(0, BOT_Y, W, C_CYAN_DD);
  g_cv.setFont(&F_LABEL);
  if (menuOpen) {
    drawButton(53, "-", C_YEL);
    drawButton(160, "NEXT  (hold: exit)", C_YEL);
    drawButton(267, "+", C_YEL);
  } else {
    drawButton(53, "VOL -", C_CYAN_D);
    drawButton(160, "MENU", C_CYAN_D);
    drawButton(267, "VOL +", C_CYAN_D);
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
  g_cv.fillRect(FRAME_X, y, W - 2 * FRAME_X, CUR_H, C_YEL_BG);
  g_cv.drawRect(FRAME_X, y, W - 2 * FRAME_X, CUR_H, C_YEL);
  int cy = y + 14;   // title line; the bar takes the lower part of the row

  // number + play mark
  char num[8];
  snprintf(num, sizeof(num), "%02d", idx + 1);
  g_cv.setFont(&F_CUR);
  g_cv.setTextDatum(middle_left);
  g_cv.setTextColor(C_YEL, C_YEL_BG);
  g_cv.drawString(num, PAD_X, cy);
  int x = PAD_X + g_cv.textWidth(num) + 10;
  if (s.state == PlayerState::Playing) g_cv.fillTriangle(x, cy - 6, x, cy + 6, x + 8, cy, C_GRN);
  else g_cv.fillRect(x, cy - 5, 9, 10, C_RED);
  x += 18;

  // title: marquee when it does not fit
  char title[cfg::MAX_NAME];
  baseTitle(library::at(idx).name, title, sizeof(title));
  int maxW = W - PAD_X - x;
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
    g_cv.setClipRect(x, y, maxW, CUR_H - 8);
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

  // progress bar, its own line under the title, neutral track for contrast
  int bx = PAD_X, bw = W - 2 * PAD_X, by = y + CUR_H - 7, bh = 3;
  g_cv.fillRect(bx, by, bw, bh, C_TRACK);
  if (s.durSec) {
    int p = (int)((uint64_t)bw * s.posSec / s.durSec);
    g_cv.fillRect(bx, by, p > bw ? bw : p, bh, C_GRN);
  }

}

void drawOtherRow(int idx, int y, int dist, bool aboveCurrent) {
  int cy = y + ROW_H / 2 + 1;
  uint16_t c = dist == 1 ? C_CYAN_M : C_CYAN_D;
  char num[8], title[cfg::MAX_NAME];
  snprintf(num, sizeof(num), "%02d", idx + 1);
  g_cv.setFont(&F_ROW);
  g_cv.setTextDatum(middle_left);
  g_cv.setTextColor(C_CYAN_D, C_BG);
  g_cv.drawString(num, PAD_X, cy);
  baseTitle(library::at(idx).name, title, sizeof(title));
  (void)aboveCurrent;
  int tx = PAD_X + 36;
  int maxW = W - PAD_X - tx;
  clipTitle(title, maxW);
  g_cv.setTextColor(c, C_BG);
  g_cv.drawString(title, tx, cy);
}

// The window is centred on the playing track plus the browse offset. Lists longer than
// the window wrap around their ends; shorter ones leave blank rows beyond the ends, and a
// drag brings the hidden files into view (the centre is clamped to the list).
int trackAtRow(int r, int centre, int n) {
  int idx = centre + (r - CENTER_ROW);
  if (n > ROWS) return ((idx % n) + n) % n;
  return (idx >= 0 && idx < n) ? idx : -1;
}

int centreFor(int playing, int browse, int n) {
  if (n > ROWS) return (((playing + browse) % n) + n) % n;
  int c = playing + browse;
  return c < 0 ? 0 : (c >= n ? n - 1 : c);
}

int rowOfTrack(int track, int centre, int n) {
  int d = track - centre;
  if (n > ROWS) {
    d = ((d % n) + n) % n;
    if (d > n / 2) d -= n;
  }
  return CENTER_ROW + d;
}

// pixel y of a track's row in the layout for a given window centre
int yOfTrack(int track, int playing, int centre, int n) {
  return rowY(rowOfTrack(track, centre, n), rowOfTrack(playing, centre, n));
}

// change the row offset without moving the picture: the shift absorbs the layout change
// exactly, including the taller playing row moving through the window
void setBrowse(int browse, int playing, int n) {
  if (n <= ROWS) {
    if (playing + browse < 0) browse = -playing;
    if (playing + browse > n - 1) browse = n - 1 - playing;
  }
  if (browse == g_browse) return;
  int oldC = centreFor(playing, g_browse, n), newC = centreFor(playing, browse, n);
  g_shift += yOfTrack(playing, playing, oldC, n) - yOfTrack(playing, playing, newC, n);
  g_browse = browse;
  g_viewGen++;
}

void drawList(const PlayerSnapshot& s, const UiStatus& st, uint32_t now) {
  g_cv.fillRect(0, LIST_Y, W, BOT_Y - LIST_Y, C_BG);
  int n = (int)st.trackCount;
  if (!st.sdMounted || n == 0) {
    drawEmpty(st);
    return;
  }
  int cur = s.track < 0 ? 0 : s.track;
  if (cur != g_lastTrack) {       // the track changed: recentre on it at once
    g_lastTrack = cur;
    g_browse = 0;
    g_shift = 0;
    g_anim = false;
    g_viewGen++;
  }
  if (g_anim) {                    // ease the shift back to rest
    g_shift *= 0.72f;
    if (g_shift < 0.6f && g_shift > -0.6f) { g_shift = 0; g_anim = false; }
    g_viewGen++;
  }
  int centre = centreFor(cur, g_browse, n);
  g_playRow = rowOfTrack(cur, centre, n);
  int shift = (int)lroundf(g_shift);
  int extra = (shift < 0 ? -shift : shift) / ROW_H + 2;   // rows beyond the window the shift can expose
  g_cv.setClipRect(0, LIST_Y, W, BOT_Y - LIST_Y);
  for (int r = -extra; r < ROWS + extra; ++r) {
    if (r == g_playRow) continue;
    int idx = trackAtRow(r, centre, n);
    if (idx < 0) continue;
    int y = rowY(r, g_playRow) + shift;
    if (y + ROW_H <= LIST_Y || y >= BOT_Y) continue;
    int dist = r < g_playRow ? g_playRow - r : r - g_playRow;
    drawOtherRow(idx, y, dist, r == g_playRow - 1);
  }
  int py = rowY(g_playRow, g_playRow) + shift;
  if (py + CUR_H > LIST_Y && py < BOT_Y) drawCurrentRow(s, cur, py, now);
  g_cv.clearClipRect();
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
    int cy = y + ROW_H / 2 + 1;
    bool isSel = (i == sel);
    bool edit = menu::editable(i);
    uint16_t bg = isSel ? C_YEL_BG : C_BG;
    if (isSel) {
      g_cv.fillRect(FRAME_X, y, W - 2 * FRAME_X, ROW_H, C_YEL_BG);
      g_cv.drawRect(FRAME_X, y, W - 2 * FRAME_X, ROW_H, C_YEL);
    }
    char lab[24], val[40];
    menu::label(i, lab, sizeof(lab));
    menu::value(i, val, sizeof(val));
    g_cv.setFont(&F_ROW);
    g_cv.setTextDatum(middle_left);
    g_cv.setTextColor(isSel ? C_YEL : (edit ? C_CYAN : C_CYAN_D), bg);
    g_cv.drawString(lab, PAD_X, cy);
    g_cv.setTextDatum(middle_right);
    g_cv.setTextColor(isSel ? C_GRN : (edit ? C_GRN_D : C_CYAN_D), bg);
    g_cv.drawString(val, W - PAD_X, cy);
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
  drawBrand(M5.Display, 8, g_bootY + 9);
  M5.Display.drawFastHLine(0, g_bootY + 22, W, C_CYAN_DD);
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

// offer modal geometry
constexpr int MD_W = 272, MD_H = 132;
constexpr int MD_X = (W - MD_W) / 2, MD_Y = LIST_Y + 22;
constexpr int BTN_W = 96, BTN_H = 30, BTN_Y = MD_Y + MD_H - BTN_H - 12;
constexpr int BTN_NO_X = MD_X + 28, BTN_YES_X = MD_X + MD_W - BTN_W - 28;

void drawOffer(const UiStatus& st) {
  g_cv.fillRect(MD_X, MD_Y, MD_W, MD_H, C_BG);
  g_cv.drawRect(MD_X, MD_Y, MD_W, MD_H, C_YEL);
  g_cv.setTextDatum(middle_center);
  g_cv.setFont(&F_CUR);
  g_cv.setTextColor(C_YEL, C_BG);
  g_cv.drawString("Computer connected", W / 2, MD_Y + 24);
  g_cv.setFont(&F_ROW);
  g_cv.setTextColor(C_FG_DIM, C_BG);
  g_cv.drawString("Switch to USB drive mode?", W / 2, MD_Y + 50);
  g_cv.setFont(&F_SMALL);
  g_cv.setTextColor(C_CYAN_D, C_BG);
  char b[32];
  snprintf(b, sizeof(b), "closes in %lu s", (unsigned long)((st.offerMs + 999) / 1000));
  g_cv.drawString(b, W / 2, MD_Y + 72);
  g_cv.setFont(&F_ROW);
  g_cv.drawRect(BTN_NO_X, BTN_Y, BTN_W, BTN_H, C_CYAN_D);
  g_cv.setTextColor(C_CYAN, C_BG);
  g_cv.drawString("NO", BTN_NO_X + BTN_W / 2, BTN_Y + BTN_H / 2 + 1);
  g_cv.fillRect(BTN_YES_X, BTN_Y, BTN_W, BTN_H, C_YEL_BG);
  g_cv.drawRect(BTN_YES_X, BTN_Y, BTN_W, BTN_H, C_YEL);
  g_cv.setTextColor(C_YEL, C_YEL_BG);
  g_cv.drawString("YES", BTN_YES_X + BTN_W / 2, BTN_Y + BTN_H / 2 + 1);
}

void drawDrive(const UiStatus& st) {
  g_cv.fillRect(0, LIST_Y, W, BOT_Y - LIST_Y, C_BG);
  g_cv.setTextDatum(middle_center);
  g_cv.setFont(&F_CUR);
  g_cv.setTextColor(C_CYAN, C_BG);
  g_cv.drawString("USB DRIVE MODE", W / 2, LIST_Y + 34);
  g_cv.setFont(&F_ROW);
  g_cv.setTextColor(C_FG_DIM, C_BG);
  g_cv.drawString("the computer holds the card", W / 2, LIST_Y + 66);
  g_cv.drawString("eject it there to resume playback", W / 2, LIST_Y + 88);
  char b[48];
  snprintf(b, sizeof(b), "read %lu KB   written %lu KB", (unsigned long)(st.usbRead / 1024), (unsigned long)(st.usbWritten / 1024));
  g_cv.setFont(&F_TIME);
  g_cv.setTextColor(C_GRN, C_BG);
  g_cv.drawString(b, W / 2, LIST_Y + 122);
  g_cv.setFont(&F_SMALL);
  g_cv.setTextColor(C_CYAN_D, C_BG);
  g_cv.drawString("hold MENU to force the card back", W / 2, LIST_Y + 160);
}

void render(const PlayerSnapshot& s, const UiStatus& st) {
  uint32_t now = millis();
  drawTop(s, st);
  if (st.usbState == 2) drawDrive(st);
  else if (st.menuOpen) drawMenu();
  else drawList(s, st, now);
  if (st.usbState == 1) drawOffer(st);
  drawBottom(st.menuOpen);
  push();
}

bool animating() { return g_mqCycle != 0 || g_anim || g_touching; }
uint32_t renderPeriod() { return (g_anim || g_touching) ? 50 : (g_mqCycle != 0 ? 100 : 0); }

int trackAtY(int y, const PlayerSnapshot& s, size_t count) {
  int n = (int)count;
  if (n == 0) return -1;
  int cur = s.track < 0 ? 0 : s.track;
  int centre = centreFor(cur, g_browse, n);
  int playRow = rowOfTrack(cur, centre, n);
  int ly = y - (int)lroundf(g_shift);
  for (int r = -ROWS; r < 2 * ROWS; ++r)
    if (ly >= rowY(r, playRow) && ly < rowY(r, playRow) + rowH(r, playRow) && y >= LIST_Y && y < BOT_Y)
      return trackAtRow(r, centre, n);
  return -1;
}

void dragBy(int dy, const PlayerSnapshot& s, size_t count, uint32_t now) {
  g_lastTouch = now;
  g_touching = true;
  g_anim = false;
  int n = (int)count;
  if (n < 2) return;
  if (dy > 60 || dy < -60) return;   // first sample after a touch begins can carry a stale delta
  if (dy == 0) return;
  int cur = s.track < 0 ? 0 : s.track;
  g_shift += dy;
  for (int guard = 0; guard < 8; ++guard) {
    int before = g_browse;
    if (g_shift >= ROW_H) setBrowse(g_browse - 1, cur, n);        // content down = earlier tracks
    else if (g_shift <= -ROW_H) setBrowse(g_browse + 1, cur, n);
    else break;
    if (g_browse == before) {                                      // clamped at a list end
      if (g_shift > 0) g_shift = 0; else if (g_shift < 0) g_shift = 0;
      break;
    }
  }
  g_viewGen++;
}

void dragEnd(const PlayerSnapshot& s, size_t count, uint32_t now) {
  if (!g_touching) return;
  g_touching = false;
  g_lastTouch = now;
  int n = (int)count;
  int cur = s.track < 0 ? 0 : s.track;
  if (g_shift > ROW_H / 2) setBrowse(g_browse - 1, cur, n);
  else if (g_shift < -ROW_H / 2) setBrowse(g_browse + 1, cur, n);
  g_anim = true;
  g_viewGen++;
}

void tick(const PlayerSnapshot& s, size_t count, uint32_t now) {
  if (!g_touching && g_browse != 0 && now - g_lastTouch > RECENTRE_MS) {
    setBrowse(0, s.track < 0 ? 0 : s.track, (int)count);
    g_anim = true;
  }
}

uint32_t viewGeneration() { return g_viewGen; }

int modalHit(int x, int y) {
  if (y < BTN_Y || y >= BTN_Y + BTN_H) return -1;
  if (x >= BTN_NO_X && x < BTN_NO_X + BTN_W) return 0;
  if (x >= BTN_YES_X && x < BTN_YES_X + BTN_W) return 1;
  return -1;
}

int menuItemAtY(int y) {
  if (y < LIST_Y || y >= BOT_Y) return -1;
  int r = (y - LIST_Y) / ROW_H;
  size_t i = menuFirst() + r;
  return i < menu::count() ? (int)i : -1;
}

}  // namespace ui
