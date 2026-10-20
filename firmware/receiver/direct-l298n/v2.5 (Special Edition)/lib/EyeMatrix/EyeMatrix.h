#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "Config.h"

struct AnimState {
    unsigned long lastUpdate;
    uint8_t step;
    int16_t scanPos;
    int8_t  scanDir;

    void reset() {
        lastUpdate = 0;
        step       = 0;
        scanPos    = 0;
        scanDir    = 1;
    }
};

class EyeMatrix {
public:
    EyeMatrix();
    void begin();

    // Call every loop iteration; motorFwd is signed -255..255 for brake detection
    void update(int mode, int motorFwd, int motorTurn);

    void displaySignalLoss();
    void setBrightness(uint8_t b);

private:
    void show();
    void fillBoth(uint32_t color);
    void fadeAccent(uint8_t amount);

    // Plain GPIO brake lights
    void brakeOn();
    void brakeOff();
    void brakeUpdate(int motorFwd);

    // RC car LED modes (Special Edition)
    void modeHeadlights();        // 0 – static white + amber
    void modeTurboSweep();        // 1 – outward comet burst
    void modeLavaFlow();          // 2 – molten heat gradient drift
    void modeNeonUnderglow();     // 3 – cyan/lime breathing underglow
    void modePoliceInterceptor(); // 4 – red/blue rapid strobe
    void modePlasmaWave();        // 5 – dual sine interference ripple
    void modeSynthwaveHorizon();  // 6 – rotating pink→purple gradient
    void modeCometTrail();        // 7 – bouncing comet with cyan tail
    void modeEmberFlicker();      // 8 – per-pixel heat-map flicker
    void modeQuantumSparkle();    // 9 – deep-space random spark decay

    Adafruit_NeoPixel stripLeft;
    Adafruit_NeoPixel stripRight;

    AnimState state;
    int currentMode;

    int  prevMotorFwd;
    bool brakeActive;
    unsigned long brakeStartMs;
};
