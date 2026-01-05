#include "AppWeb.h"
#include "WebUiHtml.h"

// Forward declarations for helpers used before their definitions
static String buildStateJson();
static void sendJson(int code, const String& json);
static bool tryGetMinArg(const char* key, uint16_t& outMin);
static String buildScheduleExportJson();
static bool parseScheduleImport(const String& body, ScheduleItem* outItems, int& outCount, uint16_t& outNextId, String& outErr);

extern WebServer server;

void handleRoot() {
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("Connection", "close");
  server.send_P(200, "text/html", INDEX_HTML, INDEX_HTML_LEN);
}

static String jsonEscape(const String& in) {
  String out;
  out.reserve(in.length() + 16);
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c == '\\') out += "\\\\";
    else if (c == '"') out += "\\\"";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else out += c;
  }
  return out;
}

static String modeName(uint8_t btn) {
  switch (btn) {
    case BTN_OFF:   return "OFF";
    case BTN_TURBO: return "TURBO";
    case BTN_HEAT:  return "HEAT";
    case BTN_COOL:  return "COOL";
    case BTN_DRY:   return "DRY";
    case BTN_EXTHT: return "EXT-HEAT";
    default:        return "UNKNOWN";
  }
}

static uint8_t modeToBtn(String s) {
  s.trim(); s.toUpperCase();
  if (s == "OFF") return BTN_OFF;
  if (s == "TURBO") return BTN_TURBO;
  if (s == "HEAT") return BTN_HEAT;
  if (s == "COOL") return BTN_COOL;
  if (s == "DRY") return BTN_DRY;
  if (s == "EXT-HEAT" || s == "EXTHT") return BTN_EXTHT;
  return BTN_OFF;
}

void handleState() {
  sendJson(200, buildStateJson());
}

void handleBleConnect() {
  bool ok = bleEnsureConnected();
  sendJson(ok ? 200 : 500, String("{\"ok\":") + (ok ? "true" : "false") + "}");
}

void handleBleDisconnect() {
  bool ok = bleDisconnect();
  sendJson(ok ? 200 : 500, String("{\"ok\":") + (ok ? "true" : "false") + "}");
}

void handleCmdButton() {
  // Quick Controls call this endpoint with optional fan/temp query params.
  // Example: /api/cmd/button?name=HEAT&fan=12&temp=92&runH=8&runM=0
  uint8_t btn = modeToBtn(server.arg("name"));

  bool ok = bleEnsureConnected();
  if (ok) ok = bedjetButton(btn);

  // If optional temp/fan provided, apply them too (tiny delays help BLE reliability).
  if (ok && server.hasArg("temp")) {
    float t = server.arg("temp").toFloat();
    delay(60);
    ok = bedjetSetTempF(t);
  }
  if (ok && server.hasArg("fan")) {
    int fan = server.arg("fan").toInt();
    delay(60);
    ok = bedjetSetFan((uint8_t)constrain(fan, (int)FAN_MIN, (int)FAN_MAX));
  }

  // Optional run-time (hours/minutes). If either is provided and total > 0, set runtime.
  if (ok && (server.hasArg("runH") || server.hasArg("runM"))) {
    int h = server.hasArg("runH") ? server.arg("runH").toInt() : 0;
    int m = server.hasArg("runM") ? server.arg("runM").toInt() : 0;
    h = constrain(h, 0, 11);
    m = constrain(m, 0, 59);
    uint16_t total = (uint16_t)(h * 60 + m);
    if (total > 0) {
      delay(60);
      ok = bedjetSetRuntimeMinutes(total);
    }
  }

  sendJson(ok ? 200 : 500, String("{\"ok\":") + (ok ? "true" : "false") + "}");
}

void handleScheduleAdd() {
  if (g_schedCount >= MAX_SCHEDULE) { server.send(400, "text/plain", "Schedule full"); return; }

  uint16_t startMin = 0, stopMin = 0;
  if (!tryGetMinArg("startMin", startMin)) { server.send(400, "text/plain", "Invalid startMin"); return; }
  if (!tryGetMinArg("stopMin", stopMin))   { server.send(400, "text/plain", "Invalid stopMin"); return; }
  if (startMin == stopMin) { server.send(400, "text/plain", "Start and stop cannot be the same"); return; }

  uint8_t btn = modeToBtn(server.arg("mode"));
  int fan = server.arg("fan").toInt();
  float temp = server.arg("temp").toFloat();
  bool enabled = server.arg("enabled").toInt() != 0;

  ScheduleItem it{};
  it.id = g_nextId++;
  it.modeButton = btn;
  it.fanStep = (uint8_t)constrain(fan, (int)FAN_MIN, (int)FAN_MAX);
  it.tempF = temp;
  it.startMin = startMin;
  it.stopMin = stopMin;
  it.enabled = enabled;

  g_sched[g_schedCount++] = it;
  saveSchedule();
  sendJson(200, "{\"ok\":true}");
}

void handleScheduleUpdate() {
  String idStr = server.arg("id");
  idStr.trim();
  if (idStr.length() == 0) { server.send(400, "text/plain", "Missing id"); return; }
  uint16_t id = (uint16_t)idStr.toInt();

  int idx = -1;
  for (int i = 0; i < g_schedCount; i++) {
    if (g_sched[i].id == id) { idx = i; break; }
  }
  if (idx < 0) { server.send(404, "text/plain", "Not found"); return; }

  uint16_t startMin = 0, stopMin = 0;
  if (!tryGetMinArg("startMin", startMin)) { server.send(400, "text/plain", "Invalid startMin"); return; }
  if (!tryGetMinArg("stopMin", stopMin))   { server.send(400, "text/plain", "Invalid stopMin"); return; }
  if (startMin == stopMin) { server.send(400, "text/plain", "Start and stop cannot be the same"); return; }

  uint8_t btn = modeToBtn(server.arg("mode"));
  int fan = server.arg("fan").toInt();
  float temp = server.arg("temp").toFloat();
  bool enabled = server.arg("enabled").toInt() != 0;

  g_sched[idx].modeButton = btn;
  g_sched[idx].fanStep = (uint8_t)constrain(fan, (int)FAN_MIN, (int)FAN_MAX);
  g_sched[idx].tempF = temp;
  g_sched[idx].startMin = startMin;
  g_sched[idx].stopMin = stopMin;
  g_sched[idx].enabled = enabled;

  saveSchedule();
  sendJson(200, "{\"ok\":true}");
}

void handleScheduleDeleteOne() {
  String idStr = server.arg("id");
  idStr.trim();
  if (idStr.length() == 0) { server.send(400, "text/plain", "Missing id"); return; }
  uint16_t id = (uint16_t)idStr.toInt();

  int idx = -1;
  for (int i = 0; i < g_schedCount; i++) {
    if (g_sched[i].id == id) { idx = i; break; }
  }
  if (idx < 0) { server.send(404, "text/plain", "Not found"); return; }

  for (int i = idx; i < g_schedCount - 1; i++) g_sched[i] = g_sched[i + 1];
  g_schedCount--;

  if (g_activeIndex == idx) g_activeIndex = -1;
  else if (g_activeIndex > idx) g_activeIndex--;

  saveSchedule();
  sendJson(200, "{\"ok\":true}");
}

void handleScheduleExport() {
  sendJson(200, buildScheduleExportJson());
}

void handleScheduleImport() {
  String body = server.arg("plain");
  body.trim();
  if (body.length() == 0) { server.send(400, "text/plain", "empty body"); return; }

  ScheduleItem items[MAX_SCHEDULE];
  int count = 0;
  uint16_t nextId = 1;
  String err;
  if (!parseScheduleImport(body, items, count, nextId, err)) {
    server.send(400, "text/plain", err.length() ? err : "parse failed");
    return;
  }

  // Replace schedule
  g_schedCount = count;
  for (int i = 0; i < g_schedCount; i++) g_sched[i] = items[i];
  g_nextId = nextId;
  g_activeIndex = -1;

  saveSchedule();
  sendJson(200, String("{\"ok\":true,\"count\":") + String(count) + "}");
}

void handleSchedulePause() {
  // POST /api/schedule/pause
  // Optional form/query arg: paused=1|0|true|false. If omitted, toggles.
  bool next = !g_cfg.schedulesPaused;
  if (server.hasArg("paused")) {
    String v = server.arg("paused");
    v.trim(); v.toLowerCase();
    if (v == "1" || v == "true" || v == "yes" || v == "on") next = true;
    else if (v == "0" || v == "false" || v == "no" || v == "off") next = false;
    else { server.send(400, "text/plain", "Invalid paused"); return; }
  }

  g_cfg.schedulesPaused = next;

  // Persist so a reboot doesn't unexpectedly resume schedules.
  saveConfigToNvs(g_cfg, true);

  sendJson(200, String("{\"ok\":true,\"paused\":") + (g_cfg.schedulesPaused ? "true" : "false") + "}");
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

static uint16_t activeScheduleId() {
  if (g_activeIndex < 0 || g_activeIndex >= g_schedCount) return 0;
  return g_sched[g_activeIndex].id;
}

static bool tryGetMinArg(const char* key, uint16_t& outMin) {
  String v = server.arg(key);
  v.trim();
  if (v.length() == 0) return false;
  int n = v.toInt();
  if (n < 0 || n > 1439) return false;
  outMin = (uint16_t)n;
  return true;
}
static String buildStateJson() {
  String timeStr = nowString();

  int isDst = -1;
  int32_t tzOff = tzOffsetSecondsNowPortable(&isDst);

  String j = "{";
  j += "\"wifi_connected\":" + String(WiFi.isConnected() ? "true" : "false") + ",";
  j += "\"ip\":\"" + jsonEscape(WiFi.localIP().toString()) + "\",";
  j += "\"time\":\"" + jsonEscape(timeStr) + "\",";
  j += "\"time_valid\":" + String(timeValid() ? "true" : "false") + ",";
  j += "\"tz\":\"" + jsonEscape(g_cfg.tz) + "\",";
  j += "\"tz_offset_sec\":" + String(tzOff) + ",";
  j += "\"dst\":" + String(isDst) + ",";
  j += "\"device_name\":\"" + jsonEscape(g_cfg.deviceName) + "\",";
  j += "\"device_mac\":\"" + jsonEscape(g_cfg.bedjetMac) + "\",";
  j += "\"ble_connected\":" + String(bleIsConnected() ? "true" : "false") + ",";
  j += "\"status_summary\":\"" + jsonEscape(statusSummary()) + "\",";
  j += "\"active_schedule_id\":" + String(activeScheduleId()) + ",";
  j += "\"sched_paused\":" + String(g_cfg.schedulesPaused ? "true" : "false") + ",";
  j += "\"schedule\":[";
  for (int i = 0; i < g_schedCount; i++) {
    if (i) j += ",";
    const auto& s = g_sched[i];
    j += "{";
    j += "\"id\":" + String(s.id) + ",";
    j += "\"mode\":\"" + jsonEscape(modeName(s.modeButton)) + "\",";
    j += "\"tempF\":" + String((int)lroundf(s.tempF)) + ",";
    j += "\"fan\":" + String(s.fanStep) + ",";
    j += "\"startMin\":" + String(s.startMin) + ",";
    j += "\"stopMin\":" + String(s.stopMin) + ",";
    j += "\"start\":\"" + jsonEscape(fmtTime12(s.startMin)) + "\",";
    j += "\"stop\":\"" + jsonEscape(fmtTime12(s.stopMin)) + "\",";
    j += "\"enabled\":" + String(s.enabled ? "true" : "false");
    j += "}";
  }
  j += "]";
  j += "}";
  return j;
}
static String buildScheduleExportJson() {
  String j = "{";
  j += "\"schema\":1,";
  j += "\"exported\":\"" + jsonEscape(nowString()) + "\",";
  j += "\"device_name\":\"" + jsonEscape(g_cfg.deviceName) + "\",";
  j += "\"device_mac\":\"" + jsonEscape(g_cfg.bedjetMac) + "\",";
  j += "\"nextId\":" + String(g_nextId) + ",";
  j += "\"schedule\":[";
  for (int i = 0; i < g_schedCount; i++) {
    if (i) j += ",";
    const auto& s = g_sched[i];
    j += "{";
    j += "\"id\":" + String(s.id) + ",";
    j += "\"mode\":\"" + jsonEscape(modeName(s.modeButton)) + "\",";
    j += "\"tempF\":" + String((int)lroundf(s.tempF)) + ",";
    j += "\"fan\":" + String(s.fanStep) + ",";
    j += "\"startMin\":" + String(s.startMin) + ",";
    j += "\"stopMin\":" + String(s.stopMin) + ",";
    j += "\"enabled\":" + String(s.enabled ? "true" : "false");
    j += "}";
  }
  j += "]";
  j += "}";
  return j;
}

static int jsonSkipWs(const String& s, int i) {
  while (i < (int)s.length()) {
    char c = s[i];
    if (c == ' ' || c == '\r' || c == '\n' || c == '\t') i++;
    else break;
  }
  return i;
}

static bool jsonFindKey(const String& obj, const char* key, int& outPosAfterColon) {
  String k = String("\"") + key + "\"";
  int p = obj.indexOf(k);
  if (p < 0) return false;
  p = obj.indexOf(':', p + k.length());
  if (p < 0) return false;
  p++;
  p = jsonSkipWs(obj, p);
  outPosAfterColon = p;
  return true;
}

static bool jsonReadNumberToken(const String& s, int start, String& outTok) {
  int i = start;
  if (i >= (int)s.length()) return false;
  // allow leading sign
  if (s[i] == '-' || s[i] == '+') i++;
  bool any = false;
  while (i < (int)s.length()) {
    char c = s[i];
    if ((c >= '0' && c <= '9') || c == '.') { any = true; i++; }
    else break;
  }
  if (!any) return false;
  outTok = s.substring(start, i);
  outTok.trim();
  return outTok.length() > 0;
}

static bool jsonGetInt(const String& obj, const char* key, int& out) {
  int p;
  if (!jsonFindKey(obj, key, p)) return false;
  String tok;
  if (!jsonReadNumberToken(obj, p, tok)) return false;
  out = tok.toInt();
  return true;
}

static bool jsonGetFloat(const String& obj, const char* key, float& out) {
  int p;
  if (!jsonFindKey(obj, key, p)) return false;
  String tok;
  if (!jsonReadNumberToken(obj, p, tok)) return false;
  out = tok.toFloat();
  return true;
}

static bool jsonGetBool(const String& obj, const char* key, bool& out) {
  int p;
  if (!jsonFindKey(obj, key, p)) return false;
  if (obj.startsWith("true", p)) { out = true; return true; }
  if (obj.startsWith("false", p)) { out = false; return true; }
  return false;
}

static bool jsonGetString(const String& obj, const char* key, String& out) {
  int p;
  if (!jsonFindKey(obj, key, p)) return false;
  if (p >= (int)obj.length() || obj[p] != '"') return false;
  p++;
  String r;
  while (p < (int)obj.length()) {
    char c = obj[p++];
    if (c == '\\') {
      if (p >= (int)obj.length()) break;
      char e = obj[p++];
      if (e == '"' || e == '\\' || e == '/') r += e;
      else if (e == 'b') r += '\b';
      else if (e == 'f') r += '\f';
      else if (e == 'n') r += '\n';
      else if (e == 'r') r += '\r';
      else if (e == 't') r += '\t';
      else r += e;
      continue;
    }
    if (c == '"') break;
    r += c;
  }
  out = r;
  return true;
}

static bool parseScheduleImport(const String& body, ScheduleItem* outItems, int& outCount, uint16_t& outNextId, String& outErr) {
  outCount = 0;
  outNextId = 1;

  int pNext;
  int tmp;
  if (jsonGetInt(body, "nextId", tmp) && tmp > 0 && tmp < 65535) {
    outNextId = (uint16_t)tmp;
  }

  int arrKey = body.indexOf("\"schedule\"");
  if (arrKey < 0) { outErr = "missing schedule"; return false; }
  int a = body.indexOf('[', arrKey);
  if (a < 0) { outErr = "missing schedule array"; return false; }

  // Find matching ']'
  int depth = 0;
  int b = -1;
  for (int i = a; i < (int)body.length(); i++) {
    char c = body[i];
    if (c == '[') depth++;
    else if (c == ']') {
      depth--;
      if (depth == 0) { b = i; break; }
    }
  }
  if (b < 0) { outErr = "bad schedule array"; return false; }

  int i = a + 1;
  uint16_t maxId = 0;

  while (i < b) {
    i = jsonSkipWs(body, i);
    if (i >= b) break;
    if (body[i] == ',') { i++; continue; }
    if (body[i] != '{') { i++; continue; }

    // extract object substring using brace depth (no nested objects expected)
    int od = 0;
    int j = i;
    for (; j < b; j++) {
      char c = body[j];
      if (c == '{') od++;
      else if (c == '}') {
        od--;
        if (od == 0) { j++; break; }
      }
    }
    if (od != 0) { outErr = "bad object"; return false; }

    String obj = body.substring(i, j);
    i = j;

    ScheduleItem it{};
    it.id = 0;
    it.modeButton = BTN_OFF;
    it.fanStep = 10;
    it.tempF = 90;
    it.startMin = 0;
    it.stopMin = 0;
    it.enabled = true;

    int idv;
    if (jsonGetInt(obj, "id", idv) && idv > 0 && idv < 65535) it.id = (uint16_t)idv;

    String modeS;
    if (jsonGetString(obj, "mode", modeS)) {
      it.modeButton = modeToBtn(modeS);
    } else {
      int mb;
      if (jsonGetInt(obj, "modeButton", mb)) it.modeButton = (uint8_t)mb;
    }

    int fan;
    if (jsonGetInt(obj, "fan", fan) || jsonGetInt(obj, "fanStep", fan)) {
      it.fanStep = (uint8_t)constrain(fan, (int)FAN_MIN, (int)FAN_MAX);
    }
    float tf;
    if (jsonGetFloat(obj, "tempF", tf) || jsonGetFloat(obj, "temp", tf)) {
      it.tempF = tf;
    }
    int sm;
    if (jsonGetInt(obj, "startMin", sm) || jsonGetInt(obj, "start", sm)) {
      it.startMin = (uint16_t)constrain(sm, 0, 1439);
    }
    int em;
    if (jsonGetInt(obj, "stopMin", em) || jsonGetInt(obj, "stop", em)) {
      it.stopMin = (uint16_t)constrain(em, 0, 1439);
    }
    bool en;
    if (jsonGetBool(obj, "enabled", en)) it.enabled = en;

    if (it.id == 0) {
      // allocate stable IDs if missing
      it.id = outNextId++;
    }
    if (it.id > maxId) maxId = it.id;

    if (outCount >= MAX_SCHEDULE) { outErr = "too many items"; return false; }
    outItems[outCount++] = it;
  }

  // ensure nextId is sane even if caller didn't provide it
  if (outNextId <= maxId) outNextId = (uint16_t)(maxId + 1);
  if (outNextId == 0) outNextId = 1;

  return true;
}
void sendAndClose(int code, const char* contentType, const String& body) {
  // NOTE: Arduino WebServer is effectively single-client; browsers that keep a
  // persistent connection open can block other clients (e.g., phone can't load
  // while a PC tab is open). Force-close after every response.
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("Connection", "close");
  server.send(code, contentType, body);
  server.client().stop();
}

static void sendJson(int code, const String& json) {
  sendAndClose(code, "application/json", json);
}

static void sendText(int code, const String& text) {
  sendAndClose(code, "text/plain", text);
}