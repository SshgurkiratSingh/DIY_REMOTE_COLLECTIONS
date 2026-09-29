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
        case 0: modeXenonShimmer(); break;
        case 1: modeAngryEyes(); break;
        case 2: modeScanningPupil(); break;
        case 3: modeMeteorRain(); break;
        case 4: modeAuroraBorealis(); break;
        case 5: modeRainbowFlow(); break;
        case 6: modeHypnoticLines(); break;
        case 7: modeFireFlicker(); break;
        case 8: modePoliceStrobe(); break;
        case 9: modeCyberSparkle(); break;
        default: modeXenonShimmer(); break;
    }
    show();
}

// ----------------------------------------------------------------------------
// Animation Implementations
// ----------------------------------------------------------------------------

void EyeMatrix::modeXenonShimmer() {
    // Icy white/blue with a subtle high-frequency shimmer
    for(int i = 0; i < NUM_LEDS; i++) {
        uint8_t flicker = random(0, 40); // 0 to 40 brightness reduction
        uint8_t r = 210 - flicker; 
        uint8_t g = 240 - flicker;
        uint8_t b = 255 - (flicker / 2);
        stripLeft.setPixelColor(i, stripLeft.Color(r, g, b));
        
        flicker = random(0, 40);
        r = 210 - flicker; 
        g = 240 - flicker;
        b = 255 - (flicker / 2);
        stripRight.setPixelColor(i, stripRight.Color(r, g, b));
    }
    delay(20); // Shimmer speed
}

void EyeMatrix::modeAngryEyes() {
    if (state.step == 0) {
        fillBoth(0);
        uint32_t red = stripLeft.Color(255, 0, 0);
        for (int i = 0; i < 5; i++) {
            stripLeft.setPixelColor(i, red);
            stripRight.setPixelColor(i, red);
        }
        state.step = 1;
    }
}

void EyeMatrix::modeScanningPupil() {
    fadeBoth(50); // Fade effect to leave a trail
    
    unsigned long t = millis();
    int pos = (t / 80) % 18; 
    if (pos > 9) pos = 18 - pos; // Map to 0..9..1..0
    
    uint32_t red = stripLeft.Color(255, 0, 0);
    stripLeft.setPixelColor(pos, red);
    stripRight.setPixelColor(pos, red);
    delay(10);
}

void EyeMatrix::modeMeteorRain() {
    fadeBoth(60); // Fast fade for meteor tail
    
    unsigned long t = millis();
    int meteorSpeed = 60;
    int pos = (t / meteorSpeed) % (NUM_LEDS + 5); // Allow it to go off screen to create a pause
    
    if (pos < NUM_LEDS) {
        uint32_t meteorColor = stripLeft.Color(255, 255, 255); // White hot head
        stripLeft.setPixelColor(pos, meteorColor);
        // Mirrored effect on right eye (shoots in opposite direction)
        stripRight.setPixelColor(NUM_LEDS - 1 - pos, meteorColor);
    }
    delay(15);
}

void EyeMatrix::modeAuroraBorealis() {
    // Smoothly flowing greens, cyans, and blues using overlapping sine waves
    unsigned long t = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
        // Wave 1: Slow, wide, Green/Blue
        float wave1 = sin(t / 800.0 + i * 0.3) * 0.5 + 0.5;
        // Wave 2: Faster, tighter, Purple/Blue
        float wave2 = sin(t / 400.0 - i * 0.4) * 0.5 + 0.5;
        
        uint8_t r = wave2 * 80;               // Little bit of purple
        uint8_t g = wave1 * 255;              // Dominant green
        uint8_t b = (wave1 + wave2) * 127;    // High blue
        
        r = min(r, (uint8_t)255);
        g = min(g, (uint8_t)255);
        b = min(b, (uint8_t)255);
        
        uint32_t c = stripLeft.Color(r, g, b);
        stripLeft.setPixelColor(i, c);
        stripRight.setPixelColor(NUM_LEDS - 1 - i, c); // Mirror the aurora
    }
    delay(20);
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
    fadeBoth(40);
    
    unsigned long t = millis();
    int phase = (t / 150) % 3;
    uint32_t c = stripLeft.gamma32(stripLeft.ColorHSV((t * 15) % 65536, 255, 255));
    
    for (int i = 0; i < NUM_LEDS; i++) {
        if (i % 3 == phase) {
            stripLeft.setPixelColor(i, c);
            stripRight.setPixelColor(i, c);
        }
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
