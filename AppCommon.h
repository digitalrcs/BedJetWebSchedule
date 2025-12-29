#pragma once
#include <Arduino.h>

// Common debug logging for BLE and other subsystems
#ifndef BLE_DEBUG
#define BLE_DEBUG 1
#endif

#if BLE_DEBUG
  #define BLELOG(fmt, ...) do { Serial.printf("[BLE] " fmt "\n", ##__VA_ARGS__); } while(0)
#else
  #define BLELOG(...) do {} while(0)
#endif
