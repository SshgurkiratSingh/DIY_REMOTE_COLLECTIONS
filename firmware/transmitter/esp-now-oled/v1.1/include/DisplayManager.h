#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Config.h"

enum MenuState
{
    STATE_HUD = 0,
    STATE_TELEMETRY,
    STATE_RX_VIEWER,
    STATE_TARGET_SELECT,
    STATE_SETTINGS,
    STATE_ABOUT,
    STATE_EDIT_PARAM,
    STATE_MAX
};

class DisplayManager
{
public:
    bool begin();
    void update(const struct_message *data, bool sendStatus, MenuState state, int subIndex, const char *targetName, const SystemConfig &config, const rx_message *rxData);
    void showAPMode();

private:
    Adafruit_SSD1306 display = Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
};

#endif
