#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "Config.h"

// Encapsulates state for the various animation modes
struct AnimState {
    unsigned long lastUpdate;
    uint8_t step;
    int16_t scanPos;
    int8_t  scanDir;
    
    void reset() {
        lastUpdate = 0;
        step = 0;
        scanPos = 0;
        scanDir = 1;
    }
};

class EyeMatrix {
public:
    EyeMatrix();
    void begin();
    
    // Call in the main loop to drive the currently selected mode
    void update(int mode);
    
    // Display the signal loss alert
    void displaySignalLoss();

private:
    // Core drawing utilities
    void show();
    void fillBoth(uint32_t color);
    void fadeBoth(uint8_t amount);
    
    // Mode functions
    void modeXenonShimmer();
    void modeAngryEyes();
    void modeScanningPupil();
    void modeMeteorRain();
    void modeAuroraBorealis();
    void modeRainbowFlow();
    void modeHypnoticLines();
    void modeFireFlicker();
    void modePoliceStrobe();
    void modeCyberSparkle();

    Adafruit_NeoPixel stripLeft;
    Adafruit_NeoPixel stripRight;
    
    AnimState state;
    int currentMode;
};
