#pragma once
#include "AppCommon.h"
#include "AppConfig.h"
#include <DNSServer.h>
#include <WebServer.h>

extern DNSServer g_dns;
extern bool g_configMode;
extern uint32_t g_pendingRestartAtMs;

void startConfigPortal();
void setupWebConfigPortal();
void setupWebNormalConfigPage();
void scheduleRestart(uint32_t delayMs = 1500);
