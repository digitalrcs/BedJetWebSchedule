#pragma once
#include "AppCommon.h"

static const int MAX_SCHEDULE = 16;

struct ScheduleItem {
  uint16_t id;
  uint8_t  modeButton;   // BedjetButton
  uint8_t  fanStep;      // 0..19
  float    tempF;
  uint16_t startMin;     // minutes from midnight local (0..1439)
  uint16_t stopMin;      // minutes from midnight local (0..1439)
  bool     enabled;
};

extern ScheduleItem g_sched[MAX_SCHEDULE];
extern int g_schedCount;
extern uint16_t g_nextId;

extern int g_activeIndex;
extern uint32_t g_lastSchedulerTickMs;
