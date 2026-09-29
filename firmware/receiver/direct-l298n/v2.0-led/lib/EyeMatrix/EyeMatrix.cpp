#include "EyeMatrix.h"

EyeMatrix::EyeMatrix() : 
    currentMode(-1),
    stripLeft(NUM_LEDS, LED_PIN_LEFT, NEO_GRB + NEO_KHZ800),
    stripRight(NUM_LEDS, LED_PIN_RIGHT, NEO_GRB + NEO_KHZ800) {
    state.reset();
}

void EyeMatrix::begin() {
    stripLeft.begin();
    stripRight.begin();
    
    stripLeft.setBrightness(DEFAULT_BRIGHTNESS);
    stripRight.setBrightness(DEFAULT_BRIGHTNESS);
    
    fillBoth(0); // Black
    show();
}

void EyeMatrix::maskHiddenLEDs() {
    stripLeft.setPixelColor(3, 0);
    stripRight.setPixelColor(3, 0);
    stripLeft.setPixelColor(7, 0);
    stripRight.setPixelColor(7, 0);
}

void EyeMatrix::fillBoth(uint32_t color) {
    for (int i = 0; i < NUM_LEDS; i++) {
        stripLeft.setPixelColor(i, color);
        stripRight.setPixelColor(i, color);
    }
}

void EyeMatrix::fadeBoth(uint8_t amount) {
    for (int i = 0; i < NUM_LEDS; i++) {
        uint32_t cL = stripLeft.getPixelColor(i);
        uint8_t rL = (uint8_t)(cL >> 16);
        uint8_t gL = (uint8_t)(cL >>  8);
        uint8_t bL = (uint8_t)cL;
        rL = (rL * (255 - amount)) >> 8;
        gL = (gL * (255 - amount)) >> 8;
        bL = (bL * (255 - amount)) >> 8;
        stripLeft.setPixelColor(i, stripLeft.Color(rL, gL, bL));
        
        uint32_t cR = stripRight.getPixelColor(i);
        uint8_t rR = (uint8_t)(cR >> 16);
        uint8_t gR = (uint8_t)(cR >>  8);
        uint8_t bR = (uint8_t)cR;
        rR = (rR * (255 - amount)) >> 8;
        gR = (gR * (255 - amount)) >> 8;
        bR = (bR * (255 - amount)) >> 8;
        stripRight.setPixelColor(i, stripRight.Color(rR, gR, bR));
    }
}

void EyeMatrix::show() {
    maskHiddenLEDs();
    stripLeft.show();
    stripRight.show();
}

void EyeMatrix::update(int mode) {
    if (mode != currentMode) {
        currentMode = mode;
        state.reset();
        fillBoth(0);
    }
    
    switch (currentMode) {
        case 0: modeSolidHeadlights(); break;
        case 1: modeAngryEyes(); break;
        case 2: modeScanningPupil(); break;
        case 3: modeNaturalBlinking(); break;
        case 4: modeSleepyBreathing(); break;
        case 5: modeRainbowFlow(); break;
        case 6: modeHypnoticLines(); break;
        case 7: modeFireFlicker(); break;
        case 8: modePoliceStrobe(); break;
        case 9: modeCyberSparkle(); break;
        default: modeSolidHeadlights(); break;
    }
    show();
}

// ----------------------------------------------------------------------------
// Animation Implementations
// ----------------------------------------------------------------------------

void EyeMatrix::modeSolidHeadlights() {
    if (state.step == 0) {
        fillBoth(stripLeft.Color(255, 255, 255));
        state.step = 1;
    }
}

void EyeMatrix::modeAngryEyes() {
    if (state.step == 0) {
        fillBoth(0);
        uint32_t red = stripLeft.Color(255, 0, 0);
        for (int i = 0; i < 3; i++) {
            stripLeft.setPixelColor(i, red);
            stripRight.setPixelColor(i, red);
        }
        for (int i = 4; i < 7; i++) {
            stripLeft.setPixelColor(i, red);
            stripRight.setPixelColor(i, red);
        }
        state.step = 1;
    }
}

void EyeMatrix::modeScanningPupil() {
    fadeBoth(30); // Fade effect to leave a trail
    
    unsigned long t = millis();
    int pos = (t / 150) % 6; 
    if (pos > 2) pos = 5 - pos; // Map to 0, 1, 2, 1, 0...
    
    int col = pos;
    int led1 = col;
    int led2 = 4 + col;
    int led3 = (col == 1) ? 8 : (col == 2 ? 9 : -1);
    
    uint32_t red = stripLeft.Color(255, 0, 0);
    stripLeft.setPixelColor(led1, red);
    stripRight.setPixelColor(led1, red);
    stripLeft.setPixelColor(led2, red);
    stripRight.setPixelColor(led2, red);
    if (led3 >= 0) {
        stripLeft.setPixelColor(led3, red);
        stripRight.setPixelColor(led3, red);
    }
    delay(10); // Help stabilize the effect speed
}

void EyeMatrix::modeNaturalBlinking() {
    unsigned long t = millis() % 4000; // Blink every 4 seconds
    if (t < 200) {
        fillBoth(0);
    } else {
        fillBoth(stripLeft.Color(255, 255, 255));
    }
}

void EyeMatrix::modeSleepyBreathing() {
    float val = (sin(millis() / 500.0) + 1.0) / 2.0; // 0.0 to 1.0
    uint8_t v = val * 255;
    
    fillBoth(0);
    uint32_t c = stripLeft.gamma32(stripLeft.ColorHSV(40000, 255, v)); 
    
    for (int i = 4; i < 7;  i++) { stripLeft.setPixelColor(i, c); stripRight.setPixelColor(i, c); }
    for (int i = 8; i < 10; i++) { stripLeft.setPixelColor(i, c); stripRight.setPixelColor(i, c); }
}

void EyeMatrix::modeRainbowFlow() {
    uint16_t baseHue = millis() * 20; 
    for (int i = 0; i < NUM_LEDS; i++) {
        uint32_t c = stripLeft.gamma32(stripLeft.ColorHSV(baseHue + (i * 6553), 255, 255));
        stripLeft.setPixelColor(i, c);
        stripRight.setPixelColor(i, c);
    }
}

void EyeMatrix::modeHypnoticLines() {
    fadeBoth(30);
    
    unsigned long t = millis();
    int phase = (t / 150) % 3;
    uint32_t c = stripLeft.gamma32(stripLeft.ColorHSV((t * 15) % 65536, 255, 255));
    
    if (phase == 0) {
        for (int i = 0; i < 3; i++) { stripLeft.setPixelColor(i, c); stripRight.setPixelColor(i, c); }
    } else if (phase == 1) {
        for (int i = 4; i < 7; i++) { stripLeft.setPixelColor(i, c); stripRight.setPixelColor(i, c); }
    } else {
        for (int i = 8; i < 10; i++) { stripLeft.setPixelColor(i, c); stripRight.setPixelColor(i, c); }
    }
    delay(10);
}

void EyeMatrix::modeFireFlicker() {
    for (int i = 0; i < NUM_LEDS; i++) {
        int flicker = random(0, 150);
        uint8_t r = 255;
        uint8_t g = 255 - flicker;
        uint8_t b = (flicker > 100) ? 0 : (100 - flicker);
        stripLeft.setPixelColor(i, stripLeft.Color(r, g, b));
        
        flicker = random(0, 150);
        r = 255;
        g = 255 - flicker;
        b = (flicker > 100) ? 0 : (100 - flicker);
        stripRight.setPixelColor(i, stripRight.Color(r, g, b));
    }
    delay(20); 
}

void EyeMatrix::modePoliceStrobe() {
    fillBoth(0);
    
    unsigned long t = millis() % 600;
    
    if (t < 300) {
        // Left eye flashes red
        if ((t / 50) % 2 == 0) {
            for(int i=0; i<NUM_LEDS; i++) stripLeft.setPixelColor(i, stripLeft.Color(255, 0, 0));
        }
    } else {
        // Right eye flashes blue
        if ((t / 50) % 2 == 0) {
            for(int i=0; i<NUM_LEDS; i++) stripRight.setPixelColor(i, stripRight.Color(0, 0, 255));
        }
    }
}

void EyeMatrix::modeCyberSparkle() {
    fadeBoth(30);
    
    if (random(0, 100) < 30) {
        stripLeft.setPixelColor(random(0, NUM_LEDS), stripLeft.ColorHSV(random(0, 65535), 200, 255));
    }
    if (random(0, 100) < 30) {
        stripRight.setPixelColor(random(0, NUM_LEDS), stripRight.ColorHSV(random(0, 65535), 200, 255));
    }
    delay(10);
}

// ----------------------------------------------------------------------------
// Signal Loss Alert
// ----------------------------------------------------------------------------

void EyeMatrix::displaySignalLoss() {
    float val = (sin(millis() / 300.0) + 1.0) / 2.0; // 0.0 to 1.0
    uint8_t r = val * 255;
    fillBoth(stripLeft.Color(r, 0, 0));
    show();
}
