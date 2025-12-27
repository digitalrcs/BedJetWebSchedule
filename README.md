# BedJetWebSchedule by Dan Roberts DanRoberts@DigitalRCS.Com

ESP32 firmware that controls a **BedJet** over **Bluetooth Low Energy (BLE)** and provides a **mobile-friendly Web UI** plus an **on-device scheduler** to automate BedJet modes (heat/cool/turbo/dry/ext-heat/off) with optional fan, temperature, and runtime settings.

> Status: actively developed. Tested primarily on ESP32 builds using NimBLE-Arduino + Arduino WebServer.

---

## Features

- **Web UI** (desktop + phone)
  - Connect / Disconnect / Refresh
  - Quick controls: **OFF, HEAT, TURBO, COOL, DRY, EXT-HEAT**
  - Optional quick parameters: **Fan**, **Temp**, **Runtime (H:M)**
  - UI feedback (pressed buttons + modal progress popups)
- **Scheduler**
  - On-device, time-of-day schedule
  - Survives reboot (stored in NVS)
- **Import/Export schedules**
  - Export to JSON
  - Import JSON (Replace)
- **First-boot provisioning**
  - Setup **AP mode** (`BedJetSetup-XXXX`) with portal at `http://192.168.4.1/`
  - Configure Wi-Fi, BedJet MAC, DHCP/static, hostname
  - Recovery: Setup AP on boot if not configured (and/or forced)
- **mDNS / hostname**
  - Default hostname: `BEDJETWEB`
  - Default mDNS: `http://bedjetweb.local/` (availability depends on client OS/network)

---

## Security Notes

This project is intended for **trusted home networks**.
- No authentication is included by default.
- Do **not** expose the web interface directly to the public internet.
- Treat the device like any IoT appliance: isolate on a trusted VLAN/SSID if desired.

---

## Hardware / Requirements

- ESP32 board (WROOM / C-series depending on your build)
- BedJet with BLE (BedJet 3 commonly used)
- 2.4 GHz Wi-Fi network (Wi-Fi + BLE share 2.4 GHz spectrum)

---

## Setup (First Boot / AP Provisioning)

If the ESP32 has no saved configuration, it will start an AP:

- **SSID:** `BedJetSetup-XXXX`
- **Portal:** `http://192.168.4.1/`

### Steps

1. Power on / reset the ESP32
2. Join Wi-Fi: `BedJetSetup-XXXX`
3. Open: `http://192.168.4.1/`
4. Enter:
   - Wi-Fi SSID + password
   - BedJet MAC (format `AA:BB:CC:DD:EE:FF`)
   - DHCP or Static IP (if static: IP/mask/gateway/DNS)
   - Hostname (default `BEDJETWEB`)
5. Click **Save & Reboot**
6. Reconnect your phone/PC to your normal Wi-Fi after the setup AP disappears

### Access after reboot

Try:
- `http://bedjetweb.local/` (mDNS; not guaranteed on all Android setups)
- `http://<device-ip>/` (router DHCP list usually shows hostname `BEDJETWEB`)

---

## Normal Mode Usage

Open the main UI and:

- Click **Connect** (shows modal progress)
- Use **Quick Controls** (each action can auto-connect with retries)
- Configure schedules
- Import/Export schedules as JSON

---

## BLE Protocol Notes (high level)

Known UUIDs:

- Service: `00001000-bed0-0080-aa55-4265644a6574`
- Status notify: `00002000-bed0-0080-aa55-4265644a6574`
- Name read: `00002001-bed0-0080-aa55-4265644a6574`
- Command write: `00002004-bed0-0080-aa55-4265644a6574`
- Version read: `00002005...` / `00002006...`

Command patterns (common):
- Buttons/modes: `[0x01, <button>]`
- Set timer: `[0x02, hours, minutes]`
- Set temp: `[0x03, tempByte]`
- Set fan: `[0x07, fanIndex]`

---

## Build

### Arduino IDE (typical)

1. Install ESP32 board support (espressif)
2. Install **NimBLE-Arduino**
3. Select the correct board + flash settings
4. Compile and upload

> If you hit flash size limits, the UI may be served as **gzipped PROGMEM bytes**.

### PlatformIO (recommended)

PlatformIO makes it easier to pin library versions and run CI builds. (A `platformio.ini` can be added if you want.)

---

## Troubleshooting

### Web UI times out / can’t connect

- Confirm you’re on the same LAN/VLAN as the ESP32
- If you just came from Setup AP, your phone may still be switching networks—re-join your home Wi-Fi explicitly
- Check router DHCP clients for hostname `BEDJETWEB`
- If `.local` does not resolve on your phone, use the IP

### BLE connect flaky / “scan: 0 results”

- Move ESP32 closer to BedJet for testing
- Avoid USB 3.0 hubs/cables near the ESP32 (2.4 GHz interference)
- BedJet may advertise infrequently when idle—try again or interact with BedJet (press a button) then reconnect
- Heavy Wi-Fi traffic can reduce scan reliability; reduce UI polling frequency if needed

---

## Docs

- User Guide: `docs/BedJetWebSchedule_User_Guide.docx`
- Developer Guide: `docs/BedJetWebSchedule_Developer_Guide.docx`

---

## Contributing

PRs welcome. Please:
- Keep changes modular (UI vs BLE vs scheduler vs config)
- Add serial logs for BLE connect/scan changes
- Update docs if you add endpoints or schedule fields

Suggested commit style:
- `feat(web): ...`
- `fix(ble): ...`
- `refactor(cfg): ...`
- `docs: ...`

---
## Images
![Esp32 and Case](/Esp32AndCase.jpg)
![Phone Web Page](/PhoneWebPage.jpg)
![Desktop Web Page](/DesktopWebPage.jpg)
## License

GPL-3.0 (or update this section to match your repository license choice).

