#pragma once
#include "AppCommon.h"
#include "AppState.h"
#include "AppConfig.h"
#include "AppTime.h"
#include "AppBle.h"
#include "AppStorage.h"
#include <WebServer.h>

// HTTP handlers (registered in setupWeb in Main.cpp)
void handleRoot();
void handleState();
void handleScheduleExport();
void handleScheduleImport();
void handleSchedulePause(); // pause/resume all schedules

void handleBleConnect();
void handleBleDisconnect();
void handleCmdButton();
void handleScheduleAdd();
void handleScheduleUpdate();
void handleScheduleDeleteOne();

// Shared helper for pages that need to respond and immediately close the connection
void sendAndClose(int code, const char* contentType, const String& body);
