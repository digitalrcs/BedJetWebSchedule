#pragma once
#include "AppCommon.h"
#include "AppConfig.h"
#include "AppState.h"
#include <NimBLEDevice.h>

// BedJet commands/buttons
enum BedjetButton : uint8_t {
  BTN_OFF   = 0x01,
  BTN_COOL  = 0x02,
  BTN_HEAT  = 0x03,
  BTN_TURBO = 0x04,
  BTN_DRY   = 0x05,
  BTN_EXTHT = 0x06
};

// Fan steps (BedJet reports 0..19)
static constexpr uint8_t FAN_MIN = 0;
static constexpr uint8_t FAN_MAX = 19;

// BLE lifecycle
void setupBle();
bool bleEnsureConnected();
bool bleDisconnect();
bool bleIsConnected();
void bleLoop();

// Device commands
bool bedjetButton(uint8_t btn);
bool bedjetSetFan(uint8_t fanStep);   // 0..19
bool bedjetSetTemp(float tempF);      // Fahrenheit (alias)
bool bedjetSetTempF(float tempF);
bool bedjetSetClockNow();
bool bedjetSetRuntimeMinutes(uint16_t minutes);

// Raw status snapshot (thread-safe copy)
bool bleGetStatusSnapshot(uint8_t* out, uint16_t& outLen, uint32_t& ageMs, bool& valid);

// Status summary string (human-friendly)
String bleStatusSummary();
