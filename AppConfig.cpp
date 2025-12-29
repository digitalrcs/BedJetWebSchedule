#include "AppConfig.h"

// Configuration storage (separate from schedules)
// --------------------------- User Config (Defaults) ---------------------------
// These are only used if no saved configuration exists.
// Use the built-in AP "BedJetSetup-XXXX" to configure on first boot/reset.
const char* DEFAULT_WIFI_SSID = "YourWIFIName";
const char* DEFAULT_WIFI_PASS = "YourWifiPassword";

// Default network (static). If DHCP is selected in config, these are ignored.
IPAddress DEFAULT_LOCAL_IP(192, 168, 1, 20);
IPAddress DEFAULT_GATEWAY (192, 168, 1, 1);
IPAddress DEFAULT_SUBNET  (255, 255, 255, 0);
IPAddress DEFAULT_DNS1    (192, 168, 1, 1);
IPAddress DEFAULT_DNS2    (8, 8, 8, 8);

// Default BedJet MAC + device name
const char* DEFAULT_BEDJET_MAC  = "AA:BB:CC:DD:EE:FF";
const char* DEFAULT_DEVICE_NAME = "BedJetDeviceName";
const char* DEFAULT_HOSTNAME   = "BedJetDeviceName";
// ------------------------------------------------------------------

RuntimeConfig g_cfg;

// Store config separately from schedules
static Preferences prefsCfg;
// Helpers
bool parseIp(const String& s, IPAddress& out) {
  int parts[4] = {0,0,0,0};
  int p = 0;
  String cur;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '.') {
      if (p >= 3) return false;
      parts[p++] = cur.toInt();
      cur = "";
    } else if (isDigit((unsigned char)c)) {
      cur += c;
    } else if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      // ignore whitespace
    } else {
      return false;
    }
  }
  parts[p] = cur.toInt();
  if (p != 3) return false;
  for (int i = 0; i < 4; i++) if (parts[i] < 0 || parts[i] > 255) return false;
  out = IPAddress(parts[0], parts[1], parts[2], parts[3]);
  return true;
}
String normalizeMac(String mac) {
  mac.trim();
  mac.toUpperCase();
  mac.replace("-", ":");
  if (mac.length() == 12) {
    // expand AABBCCDDEEFF -> AA:BB:CC:DD:EE:FF
    String out;
    for (int i = 0; i < 12; i++) {
      out += mac[i];
      if (i % 2 == 1 && i != 11) out += ":";
    }
    return out;
  }
  return mac;
}
String normalizeHost(String host) {
  host.trim();
  // Replace spaces/underscores with hyphens; strip invalid characters for hostname.
  host.replace(" ", "-");
  host.replace("_", "-");
  String out;
  out.reserve(host.length());
  for (size_t i = 0; i < host.length(); i++) {
    char c = host[i];
    bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || (c == '-') ;
    if (ok) out += c;
  }
  out.trim();
  // Hostnames can't start/end with '-' (some resolvers are picky)
  while (out.startsWith("-")) out.remove(0, 1);
  while (out.endsWith("-")) out.remove(out.length() - 1, 1);
  if (out.length() == 0) out = DEFAULT_HOSTNAME;
  // Limit length to something reasonable for mDNS / DHCP hostnames
  if (out.length() > 24) out = out.substring(0, 24);
  return out;
}


bool isMacLikelyValid(const String& mac) {
  String m = mac;
  m.trim(); m.toUpperCase();
  if (m.length() != 17) return false;
  for (int i = 0; i < 17; i++) {
    if (i % 3 == 2) { if (m[i] != ':') return false; }
    else {
      char c = m[i];
      bool ok = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
      if (!ok) return false;
    }
  }
  return true;
}

static void setDefaults() {
  g_cfg.wifiSsid = DEFAULT_WIFI_SSID;
  g_cfg.wifiPass = DEFAULT_WIFI_PASS;
  g_cfg.useDhcp  = false;

  g_cfg.localIp = DEFAULT_LOCAL_IP;
  g_cfg.gateway = DEFAULT_GATEWAY;
  g_cfg.subnet  = DEFAULT_SUBNET;
  g_cfg.dns1    = DEFAULT_DNS1;
  g_cfg.dns2    = DEFAULT_DNS2;

  g_cfg.bedjetMac = DEFAULT_BEDJET_MAC;
  g_cfg.deviceName = DEFAULT_DEVICE_NAME;
  g_cfg.hostName   = DEFAULT_HOSTNAME;
}
bool loadConfig() {
  setDefaults();

  prefsCfg.begin("cfg", true);
  String ssid = prefsCfg.getString("ssid", "");
  if (ssid.length() == 0) {
    prefsCfg.end();
    return false; // not configured
  }

  g_cfg.wifiSsid = ssid;
  g_cfg.wifiPass = prefsCfg.getString("pass", g_cfg.wifiPass);
  g_cfg.useDhcp  = prefsCfg.getBool("dhcp", g_cfg.useDhcp);

  String ip  = prefsCfg.getString("ip",  "");
  String gw  = prefsCfg.getString("gw",  "");
  String sn  = prefsCfg.getString("sn",  "");
  String d1  = prefsCfg.getString("dns1","");
  String d2  = prefsCfg.getString("dns2","");

  if (ip.length())  parseIp(ip,  g_cfg.localIp);
  if (gw.length())  parseIp(gw,  g_cfg.gateway);
  if (sn.length())  parseIp(sn,  g_cfg.subnet);
  if (d1.length())  parseIp(d1,  g_cfg.dns1);
  if (d2.length())  parseIp(d2,  g_cfg.dns2);

  g_cfg.bedjetMac  = prefsCfg.getString("mac", g_cfg.bedjetMac);
  g_cfg.deviceName = prefsCfg.getString("name", g_cfg.deviceName);
  g_cfg.hostName  = prefsCfg.getString("host", g_cfg.hostName);

  prefsCfg.end();

  g_cfg.bedjetMac = normalizeMac(g_cfg.bedjetMac);
  g_cfg.hostName = normalizeHost(g_cfg.hostName);
  return true;
}
void saveConfigToNvs(const RuntimeConfig& cfg, bool keepPasswordIfBlank) {
  prefsCfg.begin("cfg", false);
  prefsCfg.putString("ssid", cfg.wifiSsid);

  if (!keepPasswordIfBlank || cfg.wifiPass.length() > 0) {
    prefsCfg.putString("pass", cfg.wifiPass);
  }

  prefsCfg.putBool("dhcp", cfg.useDhcp);

  prefsCfg.putString("ip",   cfg.localIp.toString());
  prefsCfg.putString("gw",   cfg.gateway.toString());
  prefsCfg.putString("sn",   cfg.subnet.toString());
  prefsCfg.putString("dns1", cfg.dns1.toString());
  prefsCfg.putString("dns2", cfg.dns2.toString());

  prefsCfg.putString("mac",  cfg.bedjetMac);
  prefsCfg.putString("name", cfg.deviceName);
  prefsCfg.putString("host", cfg.hostName);

  prefsCfg.end();
}
