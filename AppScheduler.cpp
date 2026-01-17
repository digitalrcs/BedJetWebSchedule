#include "AppScheduler.h"

static bool withinBlock(uint16_t nowMin, uint16_t startMin, uint16_t stopMin) {
  if (startMin == stopMin) return false;
  if (startMin < stopMin) return (nowMin >= startMin && nowMin < stopMin);
  return (nowMin >= startMin) || (nowMin < stopMin); // wraps midnight
}

static uint16_t durationMinutes(uint16_t startMin, uint16_t stopMin) {
  int d = (int)stopMin - (int)startMin;
  if (d <= 0) d += 1440;
  return (uint16_t)d;
}

static int pickActiveIndex(uint16_t nowMin) {
  for (int i = 0; i < g_schedCount; i++) {
    if (!g_sched[i].enabled) continue;
    if (withinBlock(nowMin, g_sched[i].startMin, g_sched[i].stopMin)) return i;
  }
  return -1;
}

static bool applySchedule(const ScheduleItem& it) {
  if (!bleEnsureConnected()) return false;

  bedjetSetClockNow();
  delay(40);

  if (it.modeButton == BTN_OFF) {
    return bedjetButton(BTN_OFF);
  }

  // Use "smart" mode switching to improve reliability when crossing COOL <-> HEAT-family modes.
  if (!bedjetSetModeSmart(it.modeButton)) return false;
  delay(80);

  bedjetSetFan(it.fanStep);
  delay(60);

  bedjetSetTempF(it.tempF);
  delay(60);

  // Full block duration (recommended later: remaining duration if mid-block reboot)
  uint16_t runMins = durationMinutes(it.startMin, it.stopMin);
  bedjetSetRuntimeMinutes(runMins);

  return true;
}
void schedulerTick() {
  if (!timeValid()) return;

  // Pause = do not execute schedules; BedJet remains in its current state until resumed.
  if (g_cfg.schedulesPaused) return;

  uint16_t nowMin = minutesSinceMidnight();
  int desired = pickActiveIndex(nowMin);

  if (desired == g_activeIndex) return;

  if (desired < 0) {
    if (g_activeIndex >= 0) {
      bleEnsureConnected();
      bedjetButton(BTN_OFF);
    }
    g_activeIndex = -1;
    return;
  }

  if (applySchedule(g_sched[desired])) {
    g_activeIndex = desired;
  }
}
