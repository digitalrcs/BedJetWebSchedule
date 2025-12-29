#include "AppStorage.h"

static Preferences prefs;

void saveSchedule() {
  prefs.begin("bedjet", false);
  prefs.putUInt("count", (uint32_t)g_schedCount);
  prefs.putUInt("nextId", (uint32_t)g_nextId);

  for (int i = 0; i < g_schedCount; i++) {
    char key[16];
    snprintf(key, sizeof(key), "s%02d", i);

    uint8_t blob[18];
    memset(blob, 0, sizeof(blob));
    blob[0] = (uint8_t)(g_sched[i].id & 0xFF);
    blob[1] = (uint8_t)(g_sched[i].id >> 8);
    blob[2] = g_sched[i].modeButton;
    blob[3] = g_sched[i].fanStep;
    memcpy(&blob[4], &g_sched[i].tempF, sizeof(float));
    blob[8]  = (uint8_t)(g_sched[i].startMin & 0xFF);
    blob[9]  = (uint8_t)(g_sched[i].startMin >> 8);
    blob[10] = (uint8_t)(g_sched[i].stopMin & 0xFF);
    blob[11] = (uint8_t)(g_sched[i].stopMin >> 8);
    blob[12] = g_sched[i].enabled ? 1 : 0;

    prefs.putBytes(key, blob, sizeof(blob));
  }

  for (int i = g_schedCount; i < MAX_SCHEDULE; i++) {
    char key[16];
    snprintf(key, sizeof(key), "s%02d", i);
    prefs.remove(key);
  }

  prefs.end();
}
void loadSchedule() {
  prefs.begin("bedjet", true);

  g_schedCount = (int)prefs.getUInt("count", 0);
  if (g_schedCount < 0) g_schedCount = 0;
  if (g_schedCount > MAX_SCHEDULE) g_schedCount = MAX_SCHEDULE;

  g_nextId = (uint16_t)prefs.getUInt("nextId", 1);

  for (int i = 0; i < g_schedCount; i++) {
    char key[16];
    snprintf(key, sizeof(key), "s%02d", i);

    uint8_t blob[18];
    size_t n = prefs.getBytes(key, blob, sizeof(blob));
    if (n != sizeof(blob)) continue;

    ScheduleItem it{};
    it.id = (uint16_t)blob[0] | ((uint16_t)blob[1] << 8);
    it.modeButton = blob[2];
    it.fanStep = blob[3];
    memcpy(&it.tempF, &blob[4], sizeof(float));
    it.startMin = (uint16_t)blob[8]  | ((uint16_t)blob[9]  << 8);
    it.stopMin  = (uint16_t)blob[10] | ((uint16_t)blob[11] << 8);
    it.enabled  = blob[12] ? true : false;

    g_sched[i] = it;
    if (it.id >= g_nextId) g_nextId = it.id + 1;
  }

  prefs.end();
}
