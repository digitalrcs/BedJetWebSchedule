#pragma once
#include "AppCommon.h"
#include <WiFi.h>        // IPAddress
#include <Preferences.h>

struct RuntimeConfig {
  String   wifiSsid;
  String   wifiPass;      // stored in NVS; not echoed back to UI unless changed
  bool     useDhcp;
  IPAddress localIp;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress dns1;
  IPAddress dns2;

  String   bedjetMac;     // "AA:BB:CC:DD:EE:FF"
  String   deviceName;    // used in UI/state JSON
  String   hostName;      // hostname / mDNS name (e.g., BEDJETWEB)
  String   tz;           // POSIX TZ string (e.g., EST5EDT,M3.2.0/2,M11.1.0/2)
  bool     schedulesPaused;  // pause automation (do not execute schedules)
};

extern RuntimeConfig g_cfg;

// Helpers (used by config portal + WiFi setup)
bool parseIp(const String& s, IPAddress& out);
String normalizeMac(String mac);
String normalizeHost(String host);
bool isMacLikelyValid(const String& mac);

// Load/Save configuration from NVS
bool loadConfig();
void saveConfigToNvs(const RuntimeConfig& cfg, bool keepPasswordIfBlank = true);

// Defaults (only used if no saved config exists)
extern const char* DEFAULT_WIFI_SSID;
extern const char* DEFAULT_WIFI_PASS;

extern IPAddress DEFAULT_LOCAL_IP;
extern IPAddress DEFAULT_GATEWAY;
extern IPAddress DEFAULT_SUBNET;
extern IPAddress DEFAULT_DNS1;
extern IPAddress DEFAULT_DNS2;

extern const char* DEFAULT_BEDJET_MAC;
extern const char* DEFAULT_DEVICE_NAME;
extern const char* DEFAULT_HOSTNAME;

extern const char* DEFAULT_TZ;
