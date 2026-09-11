#include "library.h"

#include <M5Unified.h>
#include <SD.h>
#include <SPI.h>

#include <algorithm>

namespace {
Track g_tracks[cfg::MAX_FILES];
size_t g_count = 0;
bool g_mounted = false;
uint32_t g_gen = 0;

const char* const kExt[] = {".mp3", ".wav", ".aac", ".m4a", ".flac", ".ogg", ".opus"};

bool hasExt(const char* name, const char* ext) {
  size_t ln = strlen(name), le = strlen(ext);
  if (ln < le) return false;
  return strcasecmp(name + ln - le, ext) == 0;
}

int numericPrefix(const char* name) {
  if (!isdigit((unsigned char)name[0])) return -1;
  int v = 0;
  const char* p = name;
  while (isdigit((unsigned char)*p) && v < 10000) v = v * 10 + (*p++ - '0');
  return v;
}
}  // namespace

namespace library {

bool mount() {
  if (g_mounted) return true;
  // The card sits on the same SPI bus as the LCD: take the pins from M5Unified's table so
  // every Core gets its own (CoreS3 36/37/35, Fire 18/23/19), CS is GPIO4 on all of them.
  SPI.begin(M5.getPin(m5::pin_name_t::sd_spi_sclk), M5.getPin(m5::pin_name_t::sd_spi_cipo),
            M5.getPin(m5::pin_name_t::sd_spi_copi), -1);
  if (!SD.begin(M5.getPin(m5::pin_name_t::sd_spi_cs), SPI, cfg::SD_SPI_HZ)) {
    SD.end();
    return false;
  }
  if (SD.cardType() == CARD_NONE) {
    SD.end();
    return false;
  }
  g_mounted = true;
  log_i("sd mounted: %llu MB", SD.cardSize() / (1024ULL * 1024ULL));
  return true;
}

void unmount() {
  if (!g_mounted) return;
  SD.end();
  g_mounted = false;
  g_count = 0;
  g_gen++;
  log_w("sd unmounted");
}

bool mounted() { return g_mounted; }

bool cardAlive() {
  if (!g_mounted) return false;
  File root = SD.open("/");
  bool ok = (bool)root;
  if (root) root.close();
  return ok;
}

bool supported(const char* name) {
  if (name[0] == '.' || name[0] == '_') return false;   // hidden files, macOS `._` forks
  for (auto e : kExt)
    if (hasExt(name, e)) return true;
  return false;
}

size_t scan() {
  g_count = 0;
  g_gen++;
  if (!g_mounted) return 0;
  File root = SD.open("/");
  if (!root || !root.isDirectory()) {
    log_e("sd root open failed");
    return 0;
  }
  for (;;) {
    File f = root.openNextFile();
    if (!f) break;
    if (!f.isDirectory()) {
      const char* n = f.name();
      if (n[0] == '/') n++;
      if (supported(n) && g_count < cfg::MAX_FILES) {
        Track& t = g_tracks[g_count++];
        strlcpy(t.name, n, sizeof(t.name));
        t.size = f.size();
        t.index = numericPrefix(n);
      }
    }
    f.close();
  }
  root.close();
  std::sort(g_tracks, g_tracks + g_count,
            [](const Track& a, const Track& b) { return strcasecmp(a.name, b.name) < 0; });
  log_i("library: %u tracks", (unsigned)g_count);
  for (size_t i = 0; i < g_count; ++i)
    log_i("  [%u] %s (%lu bytes, cue %d)", (unsigned)i, g_tracks[i].name,
          (unsigned long)g_tracks[i].size, g_tracks[i].index);
  return g_count;
}

size_t count() { return g_count; }

const Track& at(size_t i) {
  static Track none;
  return i < g_count ? g_tracks[i] : none;
}

int findByIndex(int cueIndex) {
  for (size_t i = 0; i < g_count; ++i)
    if (g_tracks[i].index == cueIndex) return (int)i;
  return -1;
}

uint32_t generation() { return g_gen; }

}  // namespace library
