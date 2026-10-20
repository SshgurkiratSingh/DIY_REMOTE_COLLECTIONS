#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include "config.h"
#include "calibration.h"

// Web portal lifecycle
void initWebPortal();
void handleWebPortalLoop();
bool isRebootPending();

// Helper to check if calibration portal is active
bool isPortalRunning();
