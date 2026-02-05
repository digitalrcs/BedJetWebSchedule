#include "Main.h"

WebServer server(80);

static bool setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  if (!g_cfg.useDhcp) {
    BLELOG("WiFi static: ip=%s gw=%s sn=%s", g_cfg.localIp.toString().c_str(), g_cfg.gateway.toString().c_str(), g_cfg.subnet.toString().c_str());
    WiFi.config(g_cfg.localIp, g_cfg.gateway, g_cfg.subnet, g_cfg.dns1, g_cfg.dns2);
  }

  String host = normalizeHost(g_cfg.hostName);
  String hostLower = host; hostLower.toLowerCase();
  WiFi.setHostname(hostLower.c_str());

  WiFi.begin(g_cfg.wifiSsid.c_str(), g_cfg.wifiPass.c_str());

  uint32_t start = millis();
  while (!WiFi.isConnected() && millis() - start < 15000) delay(250);

  if (!WiFi.isConnected()) return false;

  Serial.printf("[WIFI] Connected. IP=%s\n", WiFi.localIP().toString().c_str());

  // mDNS: http://<hostname>.local/
  if (MDNS.begin(hostLower.c_str())) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("[MDNS] http://%s.local/\n", hostLower.c_str());
  }
  return true;
}

static void setupTimeNtp() {
  // Time zone + NTP. Note: Arduino-ESP32's configTime(...) may reset TZ to UTC,
  // so set TZ *after* calling configTime to ensure localtime() reflects the selected zone.
  String tz = g_cfg.tz; tz.trim();
  if (tz.length() == 0) tz = DEFAULT_TZ;

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  setenv("TZ", tz.c_str(), 1);
  tzset();
}

void setupBle() {
  NimBLEDevice::init("BedJetESP32");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  NimBLEDevice::setSecurityAuth(false, false, false);
}

static void setupWeb() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/state", HTTP_GET, handleState);

  server.on("/api/ble/connect", HTTP_POST, handleBleConnect);
  server.on("/api/ble/disconnect", HTTP_POST, handleBleDisconnect);

  server.on("/api/cmd/button", HTTP_POST, handleCmdButton);

  server.on("/api/schedule/add", HTTP_POST, handleScheduleAdd);
  server.on("/api/schedule/update", HTTP_POST, handleScheduleUpdate);
  server.on("/api/schedule/deleteOne", HTTP_POST, handleScheduleDeleteOne);
  server.on("/api/schedule/runOne", HTTP_POST, handleScheduleRunOne);
  server.on("/api/schedule/export", HTTP_GET, handleScheduleExport);
  server.on("/api/schedule/import", HTTP_POST, handleScheduleImport);
  server.on("/api/schedule/pause", HTTP_POST, handleSchedulePause);

  setupWebNormalConfigPage();
  server.begin();
}

void setup() {
  Serial.begin(115200);
  delay(200);

  // BOOT button (GPIO0) held low at boot forces config portal
  pinMode(0, INPUT_PULLUP);
  bool forceCfg = (digitalRead(0) == LOW);

  loadSchedule();
  bool hasCfg = loadConfig();

  if (forceCfg || !hasCfg) {
    startConfigPortal();
    return;
  }

  bool wifiOk = setupWiFi();
  if (!wifiOk) {
    Serial.println("[WIFI] Not connected. Falling back to setup AP portal.");
    startConfigPortal();
    return;
  }

  setupWeb();
  setupTimeNtp();
  setupBle();
}

void loop() {
  // Pending restart (used by config portal & config page)
  if (g_pendingRestartAtMs && (int32_t)(millis() - g_pendingRestartAtMs) >= 0) {
    delay(50);
    ESP.restart();
  }

  // Config portal mode: only serve captive portal
  if (g_configMode) {
    g_dns.processNextRequest();
    server.handleClient();
    delay(5);
    return;
  }

  server.handleClient();

  // keep BLE state honest (and clear handles if link dropped)
  bleLoop();

  // scheduler tick (every 2s)
  uint32_t now = millis();
  if (now - g_lastSchedulerTickMs >= 2000) {
    g_lastSchedulerTickMs = now;
    schedulerTick();
  }

  delay(5);
}
