#include "AppBle.h"
#include "AppTime.h"
#include <WiFi.h>

static NimBLEUUID UUID_SERVICE("00001000-bed0-0080-aa55-4265644a6574");
static NimBLEUUID UUID_STATUS ("00002000-bed0-0080-aa55-4265644a6574");
static NimBLEUUID UUID_COMMAND("00002004-bed0-0080-aa55-4265644a6574");

// BedJet commands
enum BedjetCommand : uint8_t {
  CMD_BUTTON      = 0x01,
  CMD_SET_RUNTIME = 0x02, // hours, minutes
  CMD_SET_TEMP    = 0x03, // temp step
  CMD_STATUS      = 0x06,
  CMD_SET_FAN     = 0x07, // 0..19
  CMD_SET_CLOCK   = 0x08  // hours, minutes
};

// --------------------------- Globals ---------------------------
// BLE
static NimBLEClient* g_client = nullptr;
static NimBLERemoteCharacteristic* g_chrCmd = nullptr;
static NimBLERemoteCharacteristic* g_chrStatus = nullptr;
static bool g_bleConnected = false;
static bool g_bleBusy = false;

// Status snapshot (raw)
static portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
static uint8_t  g_statusBuf[96];
static uint16_t g_statusLen = 0;
static uint32_t g_lastStatusMs = 0;
static bool     g_statusValid = false;

// --------------------------- BedJet Conversions ---------------------------
// F = 0.9 * step + 32
static float stepToF(uint8_t step) { return 0.9f * (float)step + 32.0f; }
static uint8_t fToStep(float f) {
  float step = (f - 32.0f) / 0.9f;
  if (step < 0) step = 0;
  if (step > 255) step = 255;
  return (uint8_t)lroundf(step);
}

// --------------------------- Status Buffer ---------------------------
static void setStatus(const uint8_t* data, uint16_t len, bool valid) {
  portENTER_CRITICAL(&g_mux);
  g_statusLen = (len > sizeof(g_statusBuf)) ? sizeof(g_statusBuf) : len;
  memcpy(g_statusBuf, data, g_statusLen);
  g_lastStatusMs = millis();
  g_statusValid = valid;
  portEXIT_CRITICAL(&g_mux);
}

static bool getStatusSnapshotInternal(uint8_t* out, uint16_t& outLen, uint32_t& ageMs, bool& valid) {
  portENTER_CRITICAL(&g_mux);
  outLen = g_statusLen;
  memcpy(out, g_statusBuf, outLen);
  ageMs = millis() - g_lastStatusMs;
  valid = g_statusValid;
  portEXIT_CRITICAL(&g_mux);
  return outLen > 0;
}

bool bleGetStatusSnapshot(uint8_t* out, uint16_t& outLen, uint32_t& ageMs, bool& valid) {
  return getStatusSnapshotInternal(out, outLen, ageMs, valid);
}

// --------------------------- Notify Callback (no callback classes) ---------------------------
static void onStatusNotify(NimBLERemoteCharacteristic* chr, uint8_t* data, size_t length, bool isNotify) {
  (void)chr; (void)isNotify;
  if (!data || length < 4) return;
  bool valid = (length >= 18 && data[1] == 0x56 && data[3] == 0x01);
  setStatus(data, (uint16_t)length, valid);
}

// --------------------------- BLE Core ---------------------------
void bleClearHandles() { g_chrCmd = nullptr; g_chrStatus = nullptr; }

static bool bleWrite(const uint8_t* bytes, size_t len) {
  if (!g_bleConnected || !g_chrCmd || len == 0) return false;
  return g_chrCmd->writeValue(bytes, len, false);
}
bool bedjetButton(uint8_t btn) {
  uint8_t p[2] = { CMD_BUTTON, btn };
  return bleWrite(p, sizeof(p));
}

// Some BedJet firmware revisions can behave inconsistently when switching directly between
// COOL and HEAT-family modes (HEAT/TURBO/EXT-HEAT). To make schedules reliable, we optionally
// send OFF first when crossing mode families, based on the freshest status snapshot.
static uint8_t modeIdxToButton(uint8_t modeIdx) {
  switch (modeIdx) {
    case 0: return BTN_OFF;
    case 1: return BTN_HEAT;
    case 2: return BTN_TURBO;
    case 3: return BTN_EXTHT;
    case 4: return BTN_COOL;
    case 5: return BTN_DRY;
    default: return BTN_OFF;
  }
}

static bool isHeatFamily(uint8_t btn) {
  return (btn == BTN_HEAT || btn == BTN_TURBO || btn == BTN_EXTHT);
}
static bool isCoolFamily(uint8_t btn) {
  return (btn == BTN_COOL);
}
static bool isDry(uint8_t btn) {
  return (btn == BTN_DRY);
}

static bool tryGetCurrentButton(uint8_t& outBtn) {
  uint8_t snap[96];
  uint16_t slen = 0;
  uint32_t ageMs = 0;
  bool valid = false;

  if (!bleGetStatusSnapshot(snap, slen, ageMs, valid)) return false;
  if (!valid) return false;
  // mode index is observed at [9] in the 20-byte status frame (see bleStatusSummary())
  if (slen <= 9) return false;
  // Avoid acting on stale status; during reconnects we may have an old snapshot.
  if (ageMs > 5000) return false;

  outBtn = modeIdxToButton(snap[9]);
  return true;
}

bool bedjetSetModeSmart(uint8_t targetBtn) {
  if (targetBtn == BTN_OFF) return bedjetButton(BTN_OFF);

  bool doOffFirst = false;
  uint8_t curBtn = BTN_OFF;
  if (tryGetCurrentButton(curBtn)) {
    if (curBtn != targetBtn) {
      if ((isCoolFamily(curBtn) && isHeatFamily(targetBtn)) ||
          (isHeatFamily(curBtn) && isCoolFamily(targetBtn)) ||
          isDry(curBtn) || isDry(targetBtn)) {
        doOffFirst = true;
      }
    }
  } else {
    // If we don't have a fresh status snapshot, err on the side of reliability.
    // OFF->target is slightly slower but avoids mode-transition edge cases.
    if (isHeatFamily(targetBtn) || isCoolFamily(targetBtn) || isDry(targetBtn)) {
      doOffFirst = true;
    }
  }

  if (doOffFirst) {
    if (!bedjetButton(BTN_OFF)) return false;
    delay(250);
  }

  return bedjetButton(targetBtn);
}

bool bedjetSetFan(uint8_t step) {
  step = (uint8_t)constrain((int)step, (int)FAN_MIN, (int)FAN_MAX);
  uint8_t p[2] = { CMD_SET_FAN, step };
  return bleWrite(p, sizeof(p));
}

bool bedjetSetTempF(float tempF) {
  uint8_t step = fToStep(tempF);
  uint8_t p[2] = { CMD_SET_TEMP, step };
  return bleWrite(p, sizeof(p));
}

bool bedjetSetClockNow() {
  struct tm t;
  if (!getLocalTm(&t)) return false;
  uint8_t p[3] = { CMD_SET_CLOCK, (uint8_t)t.tm_hour, (uint8_t)t.tm_min };
  return bleWrite(p, sizeof(p));
}

bool bedjetSetRuntimeMinutes(uint16_t minutes) {
  if (minutes > (23 * 60 + 59)) minutes = (23 * 60 + 59);
  uint8_t hrs = minutes / 60;
  uint8_t mins = minutes % 60;
  uint8_t p[3] = { CMD_SET_RUNTIME, hrs, mins };
  return bleWrite(p, sizeof(p));
}

bool bedjetSetTemp(float tempF) {
  return bedjetSetTempF(tempF);
}
bool bleDisconnect() {
  if (g_bleBusy) return false;
  g_bleBusy = true;

  bool ok = true;
  if (g_client && g_client->isConnected()) ok = g_client->disconnect();

  g_bleConnected = false;
  bleClearHandles();

  g_bleBusy = false;
  return ok;
}

static bool bleResolveAddress(NimBLEAddress& outAddr) {
  // Best-effort scan to discover the BedJet with the correct address type.
  // Many BedJets advertise with RANDOM/Resolvable addresses; hard-coding PUBLIC vs RANDOM is unreliable.
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setActiveScan(true);
  // More aggressive scan window improves odds of catching low-duty-cycle advertisements
  scan->setInterval(80);
  scan->setWindow(60);
  scan->setDuplicateFilter(false);

  // Ensure we are not already scanning (connect is more reliable when scan is stopped).
  scan->stop();
  scan->clearResults();

  const uint32_t scanSeconds = 10;

  // NimBLE-Arduino API differences: some versions return bool from start();
  // Use start() then fetch results via getResults() for broad compatibility.
  //
  // Some BedJets advertise at a low duty-cycle when idle; do two passes before giving up.
  NimBLEScanResults results;
  for (int pass = 0; pass < 2; pass++) {
    scan->stop();
    scan->clearResults();
    scan->start(scanSeconds, false);
    results = scan->getResults();
    if (results.getCount() > 0) break;
    delay(250);
  }

  String target = g_cfg.bedjetMac;
  target.toUpperCase();

  BLELOG("scan: %d results (target=%s)", results.getCount(), target.c_str());

  // 1) Exact MAC match (preferred)
  for (int i = 0; i < results.getCount(); i++) {
    const NimBLEAdvertisedDevice* dev = results.getDevice(i); if(!dev) continue;
    String addr = String(dev->getAddress().toString().c_str());
    addr.toUpperCase();
    if (addr == target) {
      outAddr = dev->getAddress(); // includes address type
      BLELOG("resolve: exact MAC match -> %s (type=%d)", outAddr.toString().c_str(), (int)outAddr.getType());
      scan->clearResults();
      return true;
    }
  }

  // 2) Service UUID match (handles cases where BedJet uses a different/random address)
  for (int i = 0; i < results.getCount(); i++) {
    const NimBLEAdvertisedDevice* dev = results.getDevice(i); if(!dev) continue;
    if (dev->isAdvertisingService(UUID_SERVICE)) {
      outAddr = dev->getAddress();
      BLELOG("resolve: service UUID match -> %s (type=%d)", outAddr.toString().c_str(), (int)outAddr.getType());
      scan->clearResults();
      return true;
    }
  }

  // 3) Name hint match (last resort)
  for (int i = 0; i < results.getCount(); i++) {
    const NimBLEAdvertisedDevice* dev = results.getDevice(i); if(!dev) continue;
    std::string n = dev->getName();
    if (n.empty()) continue;

    String name = String(n.c_str());
    String up = name;
    up.toUpperCase();

    if (up.indexOf("BEDJET") >= 0) {
      outAddr = dev->getAddress();
      BLELOG("resolve: name hint match (%s) -> %s (type=%d)", name.c_str(), outAddr.toString().c_str(), (int)outAddr.getType());
      scan->clearResults();
      return true;
    }
  }

  BLELOG("resolve: no match");
  scan->clearResults();
  return false;
}

static bool bleConnect() {
  if (g_bleBusy) return false;
  g_bleBusy = true;

  // Fast path
  if (g_client && g_client->isConnected() && g_chrCmd) {
    g_bleConnected = true;
    g_bleBusy = false;
    return true;
  }

  // Stop scan before connecting (NimBLE is more reliable this way)
  NimBLEDevice::getScan()->stop();

  const int MAX_TRIES = 6;
  bool ok = false;

  for (int attempt = 0; attempt < MAX_TRIES && !ok; attempt++) {
    BLELOG("connect: attempt %d/%d", attempt + 1, MAX_TRIES);

    if (!g_client) {
      g_client = NimBLEDevice::createClient();
      g_client->setConnectTimeout(15);

      // Connection params: keep them conservative, but not overly slow.
      // min/max in 1.25ms units -> 30ms..60ms
      g_client->setConnectionParams(24, 48, 0, 300);
    }

    // If partially connected, reset cleanly
    if (g_client->isConnected()) {
      g_client->disconnect();
      delay(200);
      bleClearHandles();
    }

    bool connected = false;

    // Prefer discovered address/type from scan
    NimBLEAddress addr(std::string(g_cfg.bedjetMac.c_str()), BLE_ADDR_PUBLIC);
    if (bleResolveAddress(addr)) {
      BLELOG("connect: trying resolved %s (type=%d)", addr.toString().c_str(), (int)addr.getType());
      connected = g_client->connect(addr);
    }

    // Fallback: explicit addr types with the configured MAC
    if (!connected) {
      for (int t = 0; t < 2 && !connected; t++) {
        uint8_t addrType = (t == 0) ? BLE_ADDR_PUBLIC : BLE_ADDR_RANDOM;
        NimBLEAddress a(std::string(g_cfg.bedjetMac.c_str()), addrType);
        BLELOG("connect: fallback try %s type=%d", a.toString().c_str(), (int)addrType);
        connected = g_client->connect(a);
        if (!connected) delay(350);
      }
    }

    if (!connected) {
      BLELOG("connect: failed (no link), resetting client");
      NimBLEDevice::deleteClient(g_client);
      g_client = nullptr;
      delay(900);
      continue;
    }

    BLELOG("connect: linked, discovering service/characteristics");
    NimBLERemoteService* svc = g_client->getService(UUID_SERVICE);
    if (!svc) {
      BLELOG("connect: service not found");
      bleDisconnect();
      delay(450);
      continue;
    }

    g_chrCmd = svc->getCharacteristic(UUID_COMMAND);
    g_chrStatus = svc->getCharacteristic(UUID_STATUS);
    if (!g_chrCmd || !g_chrStatus) {
      BLELOG("connect: characteristic(s) missing cmd=%d status=%d", (int)(g_chrCmd != nullptr), (int)(g_chrStatus != nullptr));
      bleDisconnect();
      delay(450);
      continue;
    }

    if (g_chrStatus->canNotify()) {
      BLELOG("connect: subscribing to status notifications");
      g_chrStatus->subscribe(true, onStatusNotify);
    } else {
      BLELOG("connect: status char cannot notify");
    }

    g_bleConnected = true;
    bedjetSetClockNow();
    ok = true;
    BLELOG("connect: OK");
  }

  if (!ok) {
    BLELOG("connect: giving up");
    g_bleConnected = false;
    bleClearHandles();
  }

  g_bleBusy = false;
  return ok;
}
bool bleEnsureConnected() {
  const uint32_t waitMs = 4000;

  // If another BLE operation is in progress, wait briefly rather than failing immediately.
  uint32_t start = millis();
  while (g_bleBusy && (millis() - start) < waitMs) {
    delay(25);
  }
  return bleConnect();
}

bool bleIsConnected() {
  // Internal handles are file-static in this compilation unit.
  return (g_client && g_client->isConnected() && g_chrCmd != nullptr);
}

void bleLoop() {
  // Keep BLE state honest: clear handles if the underlying connection dropped.
  if (g_client && g_bleConnected && !g_client->isConnected()) {
    g_bleConnected = false;
    bleClearHandles();
  }
}
static uint16_t activeScheduleId() {
  if (g_activeIndex < 0 || g_activeIndex >= g_schedCount) return 0;
  return g_sched[g_activeIndex].id;
}
static String statusSummary() {
  uint8_t snap[96];
  uint16_t slen;
  uint32_t age;
  bool valid;

  if (!bleGetStatusSnapshot(snap, slen, age, valid) || !valid) return "No status yet";

  // Observed BedJet status frame (20 bytes):
  //   [4]=hours, [5]=minutes, [6]=seconds (time remaining)
  //   [7]=actual temp byte, [8]=set temp byte
  //   [9]=mode index (0..5)
  //   [10]=fan step (0..19)
  uint8_t modeIdx = (slen > 9)  ? snap[9]  : 0;
  uint8_t fanStep = (slen > 10) ? snap[10] : 0;

  // Temperature decode: matches common community reverse-engineering & your Python script.
  auto decodeTempF = [](uint8_t b) -> int {
    int x = (int)b - 0x26;
    // F ~= (x + 66) - (x/9)
    float f = (float)(x + 66) - ((float)x / 9.0f);
    return (int)lroundf(f);
  };

  int airF = (slen > 7) ? decodeTempF(snap[7]) : 0;
  int tgtF = (slen > 8) ? decodeTempF(snap[8]) : 0;

  int fanPct = 5 + 5 * (int)fanStep; // 0->5%, 19->100%

  const char* modeStr = "unknown";
  switch (modeIdx) {
    case 0: modeStr = "off"; break;
    case 1: modeStr = "heat"; break;
    case 2: modeStr = "turbo"; break;
    case 3: modeStr = "ext-heat"; break;
    case 4: modeStr = "cool"; break;
    case 5: modeStr = "dry"; break;
    default: break;
  }

  char buf[192];
  snprintf(buf, sizeof(buf),
           "mode=%s(%u) fan=%d%% target=%dF air=%dF remaining=%u:%02u:%02u age=%ums",
           modeStr, (unsigned)modeIdx, fanPct, tgtF, airF,
           (unsigned)((slen > 4) ? snap[4] : 0),
           (unsigned)((slen > 5) ? snap[5] : 0),
           (unsigned)((slen > 6) ? snap[6] : 0),
           (unsigned)age);
  return String(buf);
}

String bleStatusSummary() {
  return statusSummary();
}
