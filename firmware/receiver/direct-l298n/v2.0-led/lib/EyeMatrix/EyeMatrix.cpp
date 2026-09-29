#include "EyeMatrix.h"

EyeMatrix::EyeMatrix() : currentMode(-1) {
    state.reset();
}

void EyeMatrix::begin() {
    FastLED.addLeds<LED_TYPE, LED_PIN_LEFT, COLOR_ORDER>(ledsLeft, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.addLeds<LED_TYPE, LED_PIN_RIGHT, COLOR_ORDER>(ledsRight, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(DEFAULT_BRIGHTNESS);
    fillBoth(CRGB::Black);
    show();
}

void EyeMatrix::setBrightness(uint8_t b) {
    FastLED.setBrightness(b);
}

void EyeMatrix::maskHiddenLEDs() {
    ledsLeft[3] = ledsRight[3] = CRGB::Black;
    ledsLeft[7] = ledsRight[7] = CRGB::Black;
}

void EyeMatrix::fillBoth(CRGB color) {
    fill_solid(ledsLeft, NUM_LEDS, color);
    fill_solid(ledsRight, NUM_LEDS, color);
}

void EyeMatrix::fadeBoth(uint8_t amount) {
    for (int i = 0; i < NUM_LEDS; i++) {
        ledsLeft[i].fadeToBlackBy(amount);
        ledsRight[i].fadeToBlackBy(amount);
    }
}

void EyeMatrix::show() {
    maskHiddenLEDs();
    FastLED.show();
}

void EyeMatrix::update(int mode) {
    if (mode != currentMode) {
        currentMode = mode;
        state.reset();
        fillBoth(CRGB::Black);
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
// Animation Implementations (Using FastLED math functions)
// ----------------------------------------------------------------------------

void EyeMatrix::modeSolidHeadlights() {
    // Solid white, drawn once. 
    if (state.step == 0) {
        fillBoth(CRGB::White);
        state.step = 1;
    }
}

void EyeMatrix::modeAngryEyes() {
    if (state.step == 0) {
        fillBoth(CRGB::Black);
        // Sharp angled brow in deep red
        for (int i = 0; i < 3; i++) {
            ledsLeft[i] = ledsRight[i] = CRGB::Red;
        }
        for (int i = 4; i < 7; i++) {
            ledsLeft[i] = ledsRight[i] = CRGB::Red;
        }
        state.step = 1;
    }
}

void EyeMatrix::modeScanningPupil() {
    // Use FastLED's triwave8 for a smooth bouncing coordinate (0 to 255)
    uint8_t pos = triwave8(millis() / 4); // Adjust divisor for speed
    
    // Map 0-255 to 3 columns (0, 1, 2)
    uint8_t col = scale8(pos, 3);
    
    fadeBoth(100);
    
    int led1 = col;            // Line 1: idx 0,1,2
    int led2 = 4 + col;        // Line 2: idx 4,5,6
    int led3 = (col == 1) ? 8 : (col == 2 ? 9 : -1);
    
    ledsLeft[led1] = ledsRight[led1] = CRGB::Red;
    ledsLeft[led2] = ledsRight[led2] = CRGB::Red;
    if (led3 >= 0) ledsLeft[led3] = ledsRight[led3] = CRGB::Red;
}

void EyeMatrix::modeNaturalBlinking() {
    uint32_t t = millis();
    // Use a slow beat to trigger the blink
    uint8_t beat = beat8(20); // 20 BPM
    
    // If the beat wraps around (0-20 region), close the eye
    if (beat < 20) {
        fillBoth(CRGB::Black);
    } else {
        fillBoth(CRGB::White);
    }
}

void EyeMatrix::modeSleepyBreathing() {
    // Smooth sine-wave pulse on lower two lines only
    uint8_t val = beatsin8(15, 30, 255); // 15 BPM, range 30-255
    
    fillBoth(CRGB::Black);
    for (int i = 4; i < 7;  i++) ledsLeft[i] = ledsRight[i] = CHSV(160, 255, val);
    for (int i = 8; i < 10; i++) ledsLeft[i] = ledsRight[i] = CHSV(160, 255, val);
}

void EyeMatrix::modeRainbowFlow() {
    // Continuous flowing color palette
    uint8_t baseHue = millis() / 10; // Speed
    for (int i = 0; i < NUM_LEDS; i++) {
        CRGB color = CHSV(baseHue + (i * 20), 255, 255);
        ledsLeft[i] = ledsRight[i] = color;
    }
}

void EyeMatrix::modeHypnoticLines() {
    // Smoothly cross-fading rows using FastLED math
    unsigned long t = millis();
    uint8_t phase = (t / 100) % 3;
    CRGB color = CHSV((t / 10) % 256, 255, 255);

    fadeBoth(80);

    if (phase == 0)      for (int i = 0; i < 3;  i++) ledsLeft[i] = ledsRight[i] = color;
    else if (phase == 1) for (int i = 4; i < 7;  i++) ledsLeft[i] = ledsRight[i] = color;
    else                 for (int i = 8; i < 10; i++) ledsLeft[i] = ledsRight[i] = color;
}

void EyeMatrix::modeFireFlicker() {
    // Realistic fire using Perlin noise
    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t noiseLeft = inoise8(i * 30, millis() / 3);
        uint8_t noiseRight = inoise8((i + 10) * 30, millis() / 3);
        
        // Map noise to fire colors (Hue 0-40)
        uint8_t hueLeft = map(noiseLeft, 0, 255, 0, 40);
        uint8_t hueRight = map(noiseRight, 0, 255, 0, 40);
        
        ledsLeft[i] = CHSV(hueLeft, 255, qadd8(noiseLeft, 50));
        ledsRight[i] = CHSV(hueRight, 255, qadd8(noiseRight, 50));
    }
}

void EyeMatrix::modePoliceStrobe() {
    // Rapid triple-flash alternating
    uint8_t beat = beat8(60); // 60 BPM for the full cycle
    
    fillBoth(CRGB::Black);
    
    if (beat < 128) {
        // Left eye flashes
        if (beat % 32 < 16) {
            fill_solid(ledsLeft, NUM_LEDS, CRGB::Red);
        }
    } else {
        // Right eye flashes
        if (beat % 32 < 16) {
            fill_solid(ledsRight, NUM_LEDS, CRGB::Blue);
        }
    }
}

void EyeMatrix::modeCyberSparkle() {
    fadeBoth(60);
    // Random injection
    if (random8() < 120) {
        ledsLeft[random8(NUM_LEDS)] = CHSV(random8(), 200, 255);
    }
    if (random8() < 120) {
        ledsRight[random8(NUM_LEDS)] = CHSV(random8(), 200, 255);
    }
}

// ----------------------------------------------------------------------------
// Signal Loss Alert
// ----------------------------------------------------------------------------

void EyeMatrix::displaySignalLoss() {
    uint8_t val = beatsin8(20, 0, 150); // Slow red pulsing
    fillBoth(CRGB(val, 0, 0));
    show();
}
