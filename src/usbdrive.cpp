#include "usbdrive.h"

#include <SD.h>
#include <sd_diskio.h>
#include <USB.h>
#include <USBCDC.h>
#include <USBMSC.h>
#include <esp_mac.h>
#include <esp_heap_caps.h>
#include <rom/ets_sys.h>
#include <ff.h>
#include <diskio.h>

#include <atomic>

// The SD library only exposes single-sector raw access, one CMD17 / CMD24 per sector.
// Its diskio layer does multi-block transfers (CMD18 / CMD25) for FatFs; these two are
// non-static in sd_diskio.cpp, so declare them and use them for the 8-sector MSC chunks.
DRESULT ff_sd_read(uint8_t pdrv, uint8_t* buffer, DWORD sector, UINT count);
DRESULT ff_sd_write(uint8_t pdrv, const uint8_t* buffer, DWORD sector, UINT count);

#include "config.h"
#include "library.h"
#include "player.h"

namespace {
USBCDC g_cdc(0);
USBMSC g_msc;
std::atomic<bool> g_hostUp{false};
std::atomic<bool> g_hostEdge{false};   // STARTED since the last tick
std::atomic<bool> g_ejected{false};
std::atomic<uint32_t> g_rd{0}, g_wr{0}, g_err{0};
bool g_quiet = false;                  // serial output muted while the card is handed over
// While muted, the log goes to a RAM ring the console can dump afterwards (`dump`).
constexpr size_t RING = 8192;
char* g_ring = nullptr;
volatile size_t g_ringHead = 0;
void ringPutc(char c) { if (g_ring) g_ring[g_ringHead++ % RING] = c; }
// Sector transfers go through a DMA-capable bounce buffer: TinyUSB's own buffer is plain
// static memory and the SPI driver may want DMA-able, 4-byte aligned bytes.
uint8_t* g_xfer = nullptr;
usbdrive::State g_state = usbdrive::State::Off;
int g_pdrv = -1;                       // FatFs drive number of the card, found at begin()
uint32_t g_offerUntil = 0;
uint32_t g_gen = 1, g_lastStatGen = 0, g_lastStatAt = 0;
constexpr uint32_t OFFER_MS = 15000;
constexpr uint16_t SECTOR = 512;

void usbEvent(void*, esp_event_base_t base, int32_t id, void*) {
  if (base != ARDUINO_USB_EVENTS) return;
  switch (id) {
    case ARDUINO_USB_STARTED_EVENT: g_hostUp = true; g_hostEdge = true; break;
    case ARDUINO_USB_RESUME_EVENT:  g_hostUp = true; break;
    case ARDUINO_USB_STOPPED_EVENT:
    case ARDUINO_USB_SUSPEND_EVENT: g_hostUp = false; break;
    default: break;
  }
}

// MSC callbacks run in the TinyUSB task: hold the SPI bus lock like the audio pump does.
int32_t onRead(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
  if (offset != 0 || bufsize % SECTOR) return -1;   // TinyUSB always asks whole sectors
  uint32_t n = bufsize / SECTOR;
  SemaphoreHandle_t lock = player_busLock();
  if (lock) xSemaphoreTake(lock, portMAX_DELAY);
  bool ok;
#ifdef HP_MSC_SINGLE_SECTOR
  ok = true;
  for (uint32_t i = 0; i < n && ok; ++i) ok = SD.readRAW((uint8_t*)buffer + i * SECTOR, lba + i);
#else
  if (g_pdrv >= 0 && g_xfer && bufsize <= 4096) {
    ok = ff_sd_read(g_pdrv, g_xfer, lba, n) == RES_OK;
    if (ok) memcpy(buffer, g_xfer, bufsize);
  } else {
    ok = true;
    for (uint32_t i = 0; i < n && ok; ++i) ok = SD.readRAW((uint8_t*)buffer + i * SECTOR, lba + i);
  }
#endif
  if (lock) xSemaphoreGive(lock);
  if (ok) g_rd += bufsize; else g_err++;
  return ok ? (int32_t)bufsize : -1;
}

int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
  if (offset != 0 || bufsize % SECTOR) return -1;
  uint32_t n = bufsize / SECTOR;
  SemaphoreHandle_t lock = player_busLock();
  if (lock) xSemaphoreTake(lock, portMAX_DELAY);
  bool ok;
#ifdef HP_MSC_SINGLE_SECTOR
  ok = true;
  for (uint32_t i = 0; i < n && ok; ++i) ok = SD.writeRAW(buffer + i * SECTOR, lba + i);
#else
  if (g_pdrv >= 0 && g_xfer && bufsize <= 4096) {
    memcpy(g_xfer, buffer, bufsize);
    ok = ff_sd_write(g_pdrv, g_xfer, lba, n) == RES_OK;
  } else {
    ok = true;
    for (uint32_t i = 0; i < n && ok; ++i) ok = SD.writeRAW(buffer + i * SECTOR, lba + i);
  }
#endif
  if (lock) xSemaphoreGive(lock);
  if (ok) g_wr += bufsize; else g_err++;
  return ok ? (int32_t)bufsize : -1;
}

bool onStartStop(uint8_t, bool start, bool load_eject) {
  if (!start && load_eject) g_ejected = true;   // the computer ejected the disk
  return true;
}
}  // namespace

namespace usbdrive {

void begin() {
  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);
  static char serial[16];
  snprintf(serial, sizeof(serial), "%02X%02X%02X", mac[3], mac[4], mac[5]);
  USB.VID(0x303A);
  USB.PID(0x8001);
  USB.manufacturerName("Hemisphere");
  USB.productName("HPlayer0");
  USB.serialNumber(serial);
  USB.onEvent(usbEvent);

  g_cdc.begin(115200);
  g_cdc.setDebugOutput(true);   // log_* lines travel over the virtual serial port

  g_msc.vendorID("HPlayer");
  g_msc.productID("microSD");
  g_msc.productRevision("1.0");
  g_msc.onRead(onRead);
  g_msc.onWrite(onWrite);
  g_msc.onStartStop(onStartStop);
  g_msc.mediaPresent(false);    // a card reader with no card until the user says so
  uint32_t sectors = library::mounted() ? SD.numSectors() : 0;
  // the card's FatFs drive number: the SD library keeps it private, but the diskio layer
  // answers for any drive it holds — take the first one that reports our sector count
  g_pdrv = -1;
  for (int d = 0; d < FF_VOLUMES && sectors; ++d)
    if (sdcard_num_sectors(d) == sectors) { g_pdrv = d; break; }
  g_msc.begin(sectors ? sectors : 1, SECTOR);
  g_xfer = (uint8_t*)heap_caps_malloc(4096, MALLOC_CAP_DMA);
  g_ring = (char*)heap_caps_calloc(RING, 1, MALLOC_CAP_SPIRAM);
  USB.begin();
  log_i("usb: cdc + msc up, %lu sectors, pdrv %d", (unsigned long)sectors, g_pdrv);
}

State state() { return g_state; }
bool hostConnected() { return g_hostUp.load(); }
uint32_t bytesRead() { return g_rd.load(); }
uint32_t bytesWritten() { return g_wr.load(); }
uint32_t generation() { return g_gen; }
uint32_t ioErrors() { return g_err.load(); }

void bench(Print& out) {
  if (!library::mounted()) { out.println("bench: no card"); return; }
  static uint8_t* buf = (uint8_t*)heap_caps_malloc(8 * SECTOR, MALLOC_CAP_DMA);
  if (!buf) { out.println("bench: no buffer"); return; }
  const uint32_t sectors = 4096, base = 2048;   // 2 MB from inside the data area
  SemaphoreHandle_t lock = player_busLock();
  if (lock) xSemaphoreTake(lock, portMAX_DELAY);
  uint32_t t0 = millis(), bad = 0;
  if (g_pdrv >= 0)
    for (uint32_t s = 0; s < sectors; s += 8)
      if (ff_sd_read(g_pdrv, buf, base + s, 8) != RES_OK) bad++;
  uint32_t t1 = millis();
  for (uint32_t s = 0; s < sectors; s++)
    if (!SD.readRAW(buf, base + s)) bad++;
  uint32_t t2 = millis();
  if (lock) xSemaphoreGive(lock);
  out.printf("bench: pdrv %d, 2 MB multi-block %lu ms (%lu kB/s), single-sector %lu ms (%lu kB/s), errors %lu\n",
             g_pdrv, (unsigned long)(t1 - t0), (unsigned long)(sectors * SECTOR / (t1 - t0 ? t1 - t0 : 1)),
             (unsigned long)(t2 - t1), (unsigned long)(sectors * SECTOR / (t2 - t1 ? t2 - t1 : 1)), (unsigned long)bad);
}
uint32_t offerRemainingMs(uint32_t now) {
  return (g_state == State::Offered && (int32_t)(g_offerUntil - now) > 0) ? g_offerUntil - now : 0;
}

void enter() {
  if (g_state == State::Active) return;
  if (!library::mounted()) { log_w("usb drive: no card"); g_state = State::Off; g_gen++; return; }
  log_w("usb drive: handing the card to the computer (serial log muted meanwhile)");
  player::stop();
  delay(50);                    // let the pump close the file it was reading
  g_ejected = false;
  g_rd = 0; g_wr = 0;
  // Nowde's finding on this TinyUSB build: IN transfers get lost when two IN endpoints
  // are busy at once. Every mass-storage read is an IN transfer, so the serial output
  // stays silent while the computer holds the card (commands are still received).
  g_cdc.setDebugOutput(false);
  ets_install_putc2(ringPutc);   // keep the trace in RAM for `dump`
  g_quiet = true;
  g_msc.mediaPresent(true);
  g_state = State::Active;
  g_gen++;
}

void leave() {
  if (g_state != State::Active) { g_state = State::Off; g_gen++; return; }
  g_msc.mediaPresent(false);
  g_state = State::Off;
  g_gen++;
  delay(100);
  g_quiet = false;
  g_cdc.setDebugOutput(true);
  log_w("usb drive: card back, read %lu KB, written %lu KB, io errors %lu", (unsigned long)(g_rd / 1024),
        (unsigned long)(g_wr / 1024), (unsigned long)g_err.load());
  // fresh mount: the computer may have rewritten anything, FatFs must not trust its cache
  SemaphoreHandle_t lock = player_busLock();
  if (lock) xSemaphoreTake(lock, portMAX_DELAY);
  library::unmount();
  bool ok = library::mount();
  if (ok) library::scan();
  if (lock) xSemaphoreGive(lock);
  player::onLibraryChanged();
}

void answer(bool yes) {
  if (g_state != State::Offered) return;
  g_state = State::Off;
  g_gen++;
  if (yes) enter();
}

void tick(uint32_t now) {
  if (g_hostEdge.exchange(false) && g_state == State::Off && library::mounted() && library::count() > 0) {
    g_state = State::Offered;
    g_offerUntil = now + OFFER_MS;
    g_gen++;
    log_i("usb: computer connected, offering drive mode");
  }
  switch (g_state) {
    case State::Offered:
      if ((int32_t)(now - g_offerUntil) >= 0 || !g_hostUp.load()) { g_state = State::Off; g_gen++; }
      break;
    case State::Active:
      if (g_ejected.load() || !g_hostUp.load()) leave();
      else if (now - g_lastStatAt > 500) {   // live counters on screen, at a gentle rate
        g_lastStatAt = now;
        uint32_t stat = g_rd.load() + g_wr.load();
        if (stat != g_lastStatGen) { g_lastStatGen = stat; g_gen++; }
      }
      break;
    default:
      break;
  }
}

}  // namespace usbdrive

// the console reads the same CDC; its replies are dropped while the card is handed over
USBCDC& usbdrive_cdc() { return g_cdc; }
bool usbdrive_quiet() { return g_quiet; }
void usbdrive_dump(Print& out) {
  if (!g_ring) return;
  size_t head = g_ringHead, start = head > RING ? head - RING : 0;
  out.printf("--- ring (%u bytes)\n", (unsigned)(head - start));
  for (size_t i = start; i < head; ++i) out.write((uint8_t)g_ring[i % RING]);
  out.println("--- end");
}
