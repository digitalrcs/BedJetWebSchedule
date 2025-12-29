#pragma once
#include "AppCommon.h"
#include <time.h>

// Basic time state
bool timeValid();
String nowString();

// Portable TZ offset (seconds) without tm_gmtoff
int32_t tzOffsetSecondsNowPortable(int* outIsDst = nullptr);

// Local time helpers (used by UI/JSON/scheduler)
bool getLocalTm(struct tm* out, uint32_t timeoutMs = 150);
uint16_t minutesSinceMidnight(uint32_t timeoutMs = 150);
String fmtTime12(uint16_t minOfDay);
