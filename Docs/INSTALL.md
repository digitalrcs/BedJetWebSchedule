# BedJetWebSchedule - Installation & Setup Guide

_ESP32-S3 N16R8 (16MB Flash / 8MB PSRAM) + Arduino IDE_

## Overview
BedJetWebSchedule is ESP32 firmware that connects to a BedJet over Bluetooth Low Energy (BLE) and exposes a mobile-friendly Web UI plus a time-of-day scheduler. On first boot (or if not configured), it creates a temporary setup Wi-Fi network (AP) so you can configure your home Wi-Fi and BedJet MAC address.

## What you need
- A BedJet with BLE support (BedJet 3 commonly used)
- An ESP32-S3 N16R8 development board (16MB flash / 8MB PSRAM)
- A USB data cable (USB-C or Micro-USB depending on board)
- A computer with Arduino IDE 2.x
- A 2.4 GHz Wi-Fi network

## Step 1 - Get an ESP32-S3 N16R8 board (Amazon)
When ordering, verify the listing explicitly says **N16R8** (or equivalent):
- **16MB flash** (often shown as **N16**)
- **8MB PSRAM** (often shown as **R8**)

Common listing keywords: `ESP32-S3`, `N16R8`, `16MB Flash`, `8MB PSRAM`, `USB-C`.

## Step 2 - Download BedJetWebSchedule from GitHub
Repo: https://github.com/digitalrcs/BedJetWebSchedule

Two common ways:
1. **Download ZIP**: Click **Code** > **Download ZIP**, then unzip.
2. **Git clone**: `git clone https://github.com/digitalrcs/BedJetWebSchedule`

Open the sketch: **BedJetWebSchedule.ino**

## Step 3 - Install Arduino IDE
Install Arduino IDE 2.x from Arduino’s official site.

## Step 4 - Add ESP32 board support
1. Arduino IDE: **File** > **Preferences**
2. In **Additional Boards Manager URLs**, add:
   - `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. **Tools** > **Board** > **Boards Manager**
4. Search **esp32** and install **esp32 by Espressif Systems**

## Step 5 - Install required libraries
Install **NimBLE-Arduino**:
1. **Sketch** > **Include Library** > **Manage Libraries**
2. Search `NimBLE-Arduino`
3. Install it

If the build complains about missing libraries, install them when prompted.

## Step 6 - Select board, port, and memory/partition settings
Plug the ESP32-S3 into your PC using a **data** USB cable.

In Arduino IDE:
1. **Tools** > **Board**: `ESP32S3 Dev Module` (or closest matching ESP32-S3 board)
2. **Tools** > **Port**: select the port that appears when the board is plugged in
3. **Tools** > **Flash Size**: `16MB (128Mb)`
4. **Tools** > **PSRAM**: `Enabled` (or `OPI PSRAM` if offered)
5. **Tools** > **Partition Scheme**: choose a **16MB** scheme with a large app + filesystem

Partition guidance (goal):
- App partition: **>= ~3MB**
- Filesystem (SPIFFS/LittleFS/FATFS): **~8MB or larger**

## Step 7 - Compile and flash
1. Open **BedJetWebSchedule.ino**
2. Click **Verify** (compile)
3. Click **Upload** (flash)

If upload fails, many boards require bootloader mode: hold **BOOT**, tap **RESET**, release **BOOT**, retry upload.

## Step 8 - First-boot setup (AP provisioning)
If not configured, the ESP32 starts a setup AP:
- SSID: `BedJetSetup-XXXX`
- Portal: `http://192.168.4.1/`

Steps:
1. Join `BedJetSetup-XXXX`
2. Open `http://192.168.4.1/`
3. Enter home Wi-Fi SSID + password
4. Enter BedJet BLE MAC (`AA:BB:CC:DD:EE:FF`)
5. Choose DHCP (recommended) or set static IP
6. **Save & Reboot**

## Step 9 - Access the web UI
Try:
- `http://bedjetweb.local/` (mDNS; may not work on every phone/network)
- `http://<device-ip>/` (find IP via router DHCP client list)

## How to find the BedJet BLE MAC address
Use a BLE scanner app (e.g., **nRF Connect**) to find the BedJet advertisement and copy the MAC in `AA:BB:CC:DD:EE:FF` format.

## Troubleshooting
- **Cannot open `bedjetweb.local`**
  - Use the IP instead (mDNS is not guaranteed).
  - Confirm you’re on the same LAN/VLAN as the ESP32.
  - Check router DHCP clients for hostname `BEDJETWEB`.

- **BLE connect flaky / scan shows 0 results**
  - Move ESP32 closer to BedJet.
  - Avoid USB 3.0 hubs/cables near the ESP32.
  - If BedJet is idle, press a button on the remote then retry.

- **Upload errors**
  - Confirm correct **Tools > Port**.
  - Try a different USB cable (must be data-capable).
  - Use BOOT/RESET sequence if required.

## Security notes
This project is intended for trusted home networks and does not include authentication by default. Do not expose the web interface directly to the public internet. Consider placing it on a trusted VLAN/SSID like other IoT devices.
