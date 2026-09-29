#pragma once

#include <Arduino.h>
#include <FastLED.h>
#include "Config.h"

// Encapsulates state for the various animation modes
struct AnimState {
    unsigned long lastUpdate;
    uint8_t step;
    uint8_t hue;
    int16_t scanPos;
    int8_t  scanDir;
    
    void reset() {
        lastUpdate = 0;
        step = 0;
        hue = 0;
        scanPos = 0;
        scanDir = 1;
    }
};

class EyeMatrix {
public:
    EyeMatrix();
    void begin();
    
    // Call in the main loop to drive the currently selected mode
    void update(int mode, bool motorSync, int motorFwd, int motorTurn);
    
    // Set the overall brightness
    void setBrightness(uint8_t b);
    
    // Display the signal loss alert
    void displaySignalLoss();

private:
    // Core drawing utilities
    void show();
    void fillBoth(CRGB color);
    void fadeBoth(uint8_t amount);
    void maskHiddenLEDs();
    
    // Mode functions
    void modeSolidHeadlights();
    void modeAngryEyes();
    void modeScanningPupil();
    void modeNaturalBlinking();
    void modeSleepyBreathing();
    void modeRainbowFlow();
    void modeHypnoticLines();
    void modeFireFlicker();
    void modePoliceStrobe();
    void modeCyberSparkle();
    
    // Motor sync logic
    void modeMotorSync(int fwd, int turn);

    CRGB ledsLeft[NUM_LEDS];
    CRGB ledsRight[NUM_LEDS];
    
    AnimState state;
    int currentMode;
};
