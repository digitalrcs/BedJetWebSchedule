#include "AppWebConfig.h"
#include "AppWeb.h"
#include <WiFi.h>
#include <ESPmDNS.h>

extern WebServer server;

DNSServer g_dns;
bool g_configMode = false;
uint32_t g_pendingRestartAtMs = 0;

static String htmlEscape(const String& in) {
  String out;
  out.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default: out += c; break;
    }
  }
  return out;
}


static void sendHtml(int code, const String& html) {
  sendAndClose(code, "text/html", html);
}

static String configPageHtml(bool apMode, const String& banner) {
  // NOTE: Keep JS minimal to avoid breaking the main UI.
  String action = apMode ? "/save" : "/config/save";

  String ipMode = g_cfg.useDhcp ? "dhcp" : "static";

  String h;
  h.reserve(4200);
  h += "<!doctype html><html><head><meta charset='utf-8'/>";
  h += "<meta name='viewport' content='width=device-width,initial-scale=1'/>";
  h += "<title>BedJet ESP32 Setup</title>";
  h += "<style>";
  h += R"CSS(
  :root{
    --bg1:#071a33; --bg2:#053a5a; --card:rgba(255,255,255,.06);
    --border:rgba(255,255,255,.14); --text:#eaf2ff; --muted:rgba(234,242,255,.72);
  }
  html,body{height:100%;}
  body{
    margin:0;
    font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial,sans-serif;
    color:var(--text);
    background: radial-gradient(1200px 600px at 20% 10%, rgba(56,182,255,.18), transparent 55%),
                radial-gradient(900px 500px at 80% 20%, rgba(0,255,170,.10), transparent 60%),
                linear-gradient(160deg, var(--bg1), var(--bg2));
  }
  .wrap{ max-width:860px; margin:0 auto; padding:18px 16px 26px; }
  .topbar{ display:flex; align-items:center; justify-content:space-between; gap:12px; margin-bottom:14px; }
  .title{ font-size:20px; font-weight:900; letter-spacing:.2px; }
  .sub{ font-size:13px; color:var(--muted); margin-top:2px; }
  .card{
    background:var(--card);
    border:1px solid var(--border);
    border-radius:18px;
    box-shadow:0 10px 30px rgba(0,0,0,.25);
    padding:16px 14px 18px;
    backdrop-filter: blur(10px);
  }
  label{ display:block; font-weight:800; margin:12px 0 6px; }
  .hint{ font-size:12.5px; color:var(--muted); margin-top:6px; }
  .row{ display:flex; gap:12px; flex-wrap:wrap; align-items:flex-end; }
  .field{
    padding:10px 12px;
    border-radius:14px;
    border:1px solid rgba(255,255,255,.18);
    background:rgba(0,0,0,.18);
    color:var(--text);
    outline:none;
    font-size:14px;
    min-width:16ch;
    max-width:60ch;
  }
  .field.long{ min-width:26ch; }
  .field.short{ min-width:10ch; }
  select.field{ cursor:pointer; }
  input.field:focus, select.field:focus{
    border-color:rgba(56,182,255,.55);
    box-shadow:0 0 0 3px rgba(56,182,255,.14);
  }
  .btn{
    display:inline-flex;
    align-items:center;
    justify-content:center;
    gap:8px;
    padding:10px 14px;
    border-radius:14px;
    border:1px solid rgba(255,255,255,.16);
    background:rgba(255,255,255,.06);
    color:var(--text);
    text-decoration:none;
    font-weight:900;
    cursor:pointer;
    user-select:none;
    transition: transform .08s ease, filter .08s ease, background .15s ease;
  }
  .btn.primary{ background:rgba(56,182,255,.16); }
  .btn.danger{ background:rgba(255,80,80,.14); }
  .btn:active, .btn.pushed{ transform: translateY(2px); filter: brightness(.95); }
  .btn:focus{ outline: none; box-shadow:0 0 0 3px rgba(56,182,255,.18); }
  .actions{ display:flex; gap:10px; flex-wrap:wrap; margin-top:14px; }
  .staticFields{ margin-top:8px; padding-top:10px; border-top:1px solid rgba(255,255,255,.10); }
  .hr{ height:1px; background:rgba(255,255,255,.10); margin:12px 0; }

  @media (max-width: 720px){
    .topbar{ flex-wrap:wrap; }
    .field, .field.long, .field.short{ min-width: 0; width: 100%; max-width: 100%; }
    .row > div{ width: 100%; }
  }

  .banner{
    padding:10px 12px;
    border-radius:14px;
    background:rgba(255,195,0,.16);
    border:1px solid rgba(255,195,0,.25);
    margin-bottom:12px;
    font-weight:900;
  }
)CSS";
  h += "</style>";
  h += "</head><body>";
  String backHref = apMode ? "/" : "/";
  h += "<div class='wrap'>";
  h += "<div class='topbar'>";
  h += String("<a class='btn' href='") + backHref + "'>&larr; Back</a>";
  h += "<div>";
  h += "<div class='title'>BedJet ESP32 Setup</div>";
  h += String("<div class='sub'>") + (apMode ? "Setup access point mode" : "Configure Wi‑Fi and networking") + "</div>";
  h += "</div>";
  h += "</div>";
  h += "<div class='card'>";
  if (banner.length()) {
    h += "<div class='banner'>" + banner + "</div>";
  }
  if (apMode) {
    h += "<div class='hint'>You're connected to the setup AP. Enter your Wi‑Fi and BedJet settings, then Save &amp; Reboot.</div>";
  } else {
    h += "<div class='hint'>Change settings, then Save &amp; Reboot.</div>";
  }

  h += "<form method='POST' action='" + action + "'>";

  h += "<label>Wi‑Fi SSID</label>";
  h += "<input class='field long' name='ssid' type='text' value='" + htmlEscape(g_cfg.wifiSsid) + "' required />";

  h += "<label>Wi‑Fi Password</label>";
  h += "<input class='field long' name='pass' type='password' value='' placeholder='(leave blank to keep current)' />";
  h += "<div class='hint'>Leave blank to keep the currently saved password.</div>";

  h += "<label>IP Mode</label>";
  h += "<select class='field short' name='ipmode' id='ipmode'>";
  h += String("<option value='dhcp'") + (ipMode == "dhcp" ? " selected" : "") + ">DHCP (dynamic)</option>";
  h += String("<option value='static'") + (ipMode == "static" ? " selected" : "") + ">Static</option>";
  h += "</select>";

  h += "<div class='staticFields' id='staticFields'>";
  h += "<div class='row'>";
  h += "<div><label>Static IP</label><input class='field' name='ip' type='text' value='" + g_cfg.localIp.toString() + "' /></div>";
  h += "<div><label>Gateway</label><input class='field' name='gw' type='text' value='" + g_cfg.gateway.toString() + "' /></div>";
  h += "</div>";
  h += "<div class='row'>";
  h += "<div><label>Subnet</label><input class='field' name='sn' type='text' value='" + g_cfg.subnet.toString() + "' /></div>";
  h += "<div><label>DNS 1</label><input class='field' name='dns1' type='text' value='" + g_cfg.dns1.toString() + "' /></div>";
  h += "</div>";
  h += "<div class='row'>";
  h += "<div><label>DNS 2</label><input class='field' name='dns2' type='text' value='" + g_cfg.dns2.toString() + "' /></div>";
  h += "<div></div>";
  h += "</div>";
  h += "</div>";

  h += "<label>BedJet MAC</label>";
  h += "<input class='field long' name='mac' type='text' value='" + htmlEscape(g_cfg.bedjetMac) + "' placeholder='AA:BB:CC:DD:EE:FF' />";

  h += "<label>Device Name</label>";
  h += "<input class='field long' name='name' type='text' value='" + htmlEscape(g_cfg.deviceName) + "' />";

  h += "<label>Hostname (mDNS)</label>";
  h += "<input class='field short' name='host' type='text' value='" + htmlEscape(g_cfg.hostName) + "' placeholder='BEDJETWEB' />";
  h += "<div class='hint'>After reboot you can usually open: <b>http://"+ htmlEscape(g_cfg.hostName) +".local/</b> (PC/iOS often work; Android varies). IP always works.</div>";

  h += "<div style='margin-top:14px;display:flex;gap:10px;flex-wrap:wrap;'>";
  h += "<div class='actions'>";
  h += "<button class='btn primary' type='submit'>Save &amp; Reboot</button>";
  h += "</div>";

  h += "<div class='hint'>Tip: hold the ESP32 <b>BOOT</b> button during power‑up to force setup AP mode.</div>";

  h += "</form>";

  // small JS: show/hide static fields; keep it simple and isolated
  h += "<script>";
  h += "function upd(){var m=document.getElementById('ipmode').value;var s=document.getElementById('staticFields');if(!s)return; s.style.display=(m==='static')?'block':'none';}";
  h += "document.getElementById('ipmode').addEventListener('change',upd);upd();";
  h += "</script>";
  h += "<script>";
  h += "function pushFx(el){if(!el) return; el.classList.add(\"pushed\"); setTimeout(()=>{el.classList.remove(\"pushed\");}, 180);}";
  h += "document.querySelectorAll(\".btn\").forEach(b=>{ b.addEventListener(\"click\",()=>pushFx(b)); });";
  h += "</script>";

  h += "</div></div></body></html>";
  return h;
}

static bool applyConfigFromRequest(RuntimeConfig& outCfg) {
  RuntimeConfig c = g_cfg;

  String ssid = server.arg("ssid"); ssid.trim();
  if (ssid.length() == 0) return false;
  c.wifiSsid = ssid;

  String pass = server.arg("pass"); // may be blank (keep current)
  pass.trim();
  c.wifiPass = pass; // if blank, saveConfigToNvs will keep old

  String ipmode = server.arg("ipmode"); ipmode.trim(); ipmode.toLowerCase();
  c.useDhcp = (ipmode == "dhcp");

  if (!c.useDhcp) {
    IPAddress ip, gw, sn, d1, d2;
    String sip = server.arg("ip");  sip.trim();
    String sgw = server.arg("gw");  sgw.trim();
    String ssn = server.arg("sn");  ssn.trim();
    String sd1 = server.arg("dns1"); sd1.trim();
    String sd2 = server.arg("dns2"); sd2.trim();

    if (sip.length() && !parseIp(sip, ip)) return false;
    if (sgw.length() && !parseIp(sgw, gw)) return false;
    if (ssn.length() && !parseIp(ssn, sn)) return false;
    if (sd1.length() && !parseIp(sd1, d1)) return false;
    if (sd2.length() && !parseIp(sd2, d2)) return false;

    if (sip.length()) c.localIp = ip;
    if (sgw.length()) c.gateway = gw;
    if (ssn.length()) c.subnet = sn;
    if (sd1.length()) c.dns1 = d1;
    if (sd2.length()) c.dns2 = d2;
  }

  String mac = server.arg("mac");
  mac = normalizeMac(mac);
  if (mac.length()) {
    // allow blank, but if provided it must be valid-ish
    if (!isMacLikelyValid(mac)) return false;
    c.bedjetMac = mac;
  }

  String name = server.arg("name"); name.trim();
  if (name.length()) c.deviceName = name;

  String host = server.arg("host");
  host = normalizeHost(host);
  if (host.length()) c.hostName = host;

  outCfg = c;
  return true;
}

static String tryWifiAndGetIp(const RuntimeConfig& c, bool& ok) {
  ok = false;

  WiFi.disconnect(true, true);
  delay(200);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  if (!c.useDhcp) {
    WiFi.config(c.localIp, c.gateway, c.subnet, c.dns1, c.dns2);
  }

  WiFi.begin(c.wifiSsid.c_str(), (c.wifiPass.length() ? c.wifiPass.c_str() : g_cfg.wifiPass.c_str()));

  uint32_t start = millis();
  while (!WiFi.isConnected() && millis() - start < 9000) delay(250);

  if (!WiFi.isConnected()) return String("");

  ok = true;
  return WiFi.localIP().toString();
}

static void handleConfigGet(bool apMode) {
  sendHtml(200, configPageHtml(apMode, ""));
}


static void handleConfigSave(bool apMode) {
  RuntimeConfig newCfg;
  if (!applyConfigFromRequest(newCfg)) {
    sendHtml(400, configPageHtml(apMode, "Invalid input. Please check SSID / IP fields / MAC format."));
    return;
  }

  // Persist config (keep existing password if blank)
  saveConfigToNvs(newCfg, true);

  // Update runtime copy
  if (newCfg.wifiPass.length() == 0) newCfg.wifiPass = g_cfg.wifiPass;
  g_cfg = newCfg;

  String msg;
  msg.reserve(256);
  msg += "Saved configuration.\n";
  if (!g_cfg.useDhcp) {
    msg += "After reboot, browse: http://" + g_cfg.localIp.toString() + "/\n";
  } else {
    msg += "After reboot, the device will get an IP via DHCP.\n";
    msg += "Check Serial log or your router DHCP leases for the IP.\n";
  }
  msg += "If you\'re on a phone: when the setup Wi‑Fi drops, reconnect to your home Wi‑Fi SSID \"" + g_cfg.wifiSsid + "\" and then open the URL above.\n";
  msg += "Rebooting...";

  // IMPORTANT: Do not disrupt Wi‑Fi before responding (or the browser will hang).
  sendHtml(200, configPageHtml(apMode, msg));
  scheduleRestart(apMode ? 4500 : 1500);
}
void scheduleRestart(uint32_t delayMs) {
  g_pendingRestartAtMs = millis() + delayMs;
}
void setupWebConfigPortal() {
  server.on("/", HTTP_GET, [](){ handleConfigGet(true); });
  server.on("/save", HTTP_POST, [](){ handleConfigSave(true); });

  // Captive portal: redirect everything to /
  server.onNotFound([](){
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/");
    server.sendHeader("Connection", "close");
    server.send(302, "text/plain", "");
    server.client().stop();
  });

  server.begin();
}
void setupWebNormalConfigPage() {
  server.on("/config", HTTP_GET, [](){ handleConfigGet(false); });
  server.on("/config/save", HTTP_POST, [](){ handleConfigSave(false); });
}
void startConfigPortal() {
  g_configMode = true;

  WiFi.mode(WIFI_AP);
  uint32_t chip = (uint32_t)(ESP.getEfuseMac() & 0xFFFF);
  char ssid[32];
  snprintf(ssid, sizeof(ssid), "BedJetSetup-%04X", (unsigned int)chip);

  // Open AP (no password) to simplify first boot
  WiFi.softAP(ssid);

  IPAddress apIp = WiFi.softAPIP();
  g_dns.start(53, "*", apIp);

  setupWebConfigPortal();

  Serial.printf("[CFG] Setup AP started. SSID=%s  IP=%s\n", ssid, apIp.toString().c_str());
  Serial.printf("[CFG] Open http://%s/ in a browser\n", apIp.toString().c_str());
}