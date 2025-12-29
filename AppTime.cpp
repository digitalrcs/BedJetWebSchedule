#include "AppTime.h"

bool timeValid() {
  time_t now = time(nullptr);
  return now > 1700000000; // sanity threshold
}

bool getLocalTm(struct tm* out, uint32_t timeoutMs) {
  return getLocalTime(out, timeoutMs);
}

uint16_t minutesSinceMidnight(uint32_t timeoutMs) {
  struct tm t;
  if (!getLocalTm(&t, timeoutMs)) return 0;
  return (uint16_t)(t.tm_hour * 60 + t.tm_min);
}

static String fmt2(int v) {
  char b[8];
  snprintf(b, sizeof(b), "%02d", v);
  return String(b);
}
String nowString() {
  struct tm t;
  if (!getLocalTm(&t)) return "NTP sync pending";
  return String(1900 + t.tm_year) + "-" + fmt2(1 + t.tm_mon) + "-" + fmt2(t.tm_mday) +
         " " + fmt2(t.tm_hour) + ":" + fmt2(t.tm_min) + ":" + fmt2(t.tm_sec);
}

String fmtTime12(uint16_t minOfDay) {
  int hh24 = minOfDay / 60;
  int mm = minOfDay % 60;
  bool pm = hh24 >= 12;
  int hh12 = hh24 % 12;
  if (hh12 == 0) hh12 = 12;
  char buf[16];
  snprintf(buf, sizeof(buf), "%d:%02d %s", hh12, mm, pm ? "PM" : "AM");
  return String(buf);
}

// Portable TZ offset (seconds) without tm_gmtoff
int32_t tzOffsetSecondsNowPortable(int* outIsDst /* nullable */) {
  time_t now = time(nullptr);

  struct tm lt{};
  struct tm gt{};
  localtime_r(&now, &lt);
  gmtime_r(&now, &gt);

  if (outIsDst) *outIsDst = lt.tm_isdst;

  // Convert broken-down structs back to epoch (interpreted as local time)
  struct tm gt2 = gt;
  gt2.tm_isdst = 0;

  time_t localEpoch = mktime(&lt);
  time_t gmEpoch    = mktime(&gt2);

  return (int32_t)difftime(localEpoch, gmEpoch);
}
