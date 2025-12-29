#include "AppState.h"

ScheduleItem g_sched[MAX_SCHEDULE];
int g_schedCount = 0;
uint16_t g_nextId = 1;

int g_activeIndex = -1;
uint32_t g_lastSchedulerTickMs = 0;
