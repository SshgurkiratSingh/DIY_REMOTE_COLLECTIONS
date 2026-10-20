#include "EyeMatrix.h"

EyeMatrix::EyeMatrix() :
    stripLeft (NUM_LEDS, LED_PIN_LEFT,  NEO_GRB + NEO_KHZ800),
    stripRight(NUM_LEDS, LED_PIN_RIGHT, NEO_GRB + NEO_KHZ800),
    currentMode(-1), prevMotorFwd(0), brakeActive(false), brakeStartMs(0) {
    state.reset();
}

void EyeMatrix::begin() {
    stripLeft.begin();
    stripRight.begin();
    stripLeft.setBrightness(DEFAULT_BRIGHTNESS);
    stripRight.setBrightness(DEFAULT_BRIGHTNESS);
    fillBoth(0);
    show();

    pinMode(BRAKE_LED_L, OUTPUT);
    pinMode(BRAKE_LED_R, OUTPUT);
    brakeOff();
}

void EyeMatrix::setBrightness(uint8_t b) {
    b = max(b, (uint8_t)MIN_BRIGHTNESS);
    stripLeft.setBrightness(b);
    stripRight.setBrightness(b);
}

// ---------------------------------------------------------------------------
// Core helpers
// ---------------------------------------------------------------------------

void EyeMatrix::show() {
    stripLeft.show();
    stripRight.show();
}

void EyeMatrix::fillBoth(uint32_t color) {
    for (int i = 0; i < NUM_LEDS; i++) {
        stripLeft.setPixelColor(i, color);
        stripRight.setPixelColor(i, color);
    }
}

void EyeMatrix::fadeAccent(uint8_t amount) {
    for (int i = 0; i < NUM_LEDS; i++) {
        auto fade = [&](Adafruit_NeoPixel &s) {
            uint32_t c = s.getPixelColor(i);
            uint8_t r = ((uint8_t)(c >> 16) * (255 - amount)) >> 8;
            uint8_t g = ((uint8_t)(c >>  8) * (255 - amount)) >> 8;
            uint8_t b = ((uint8_t)(c)       * (255 - amount)) >> 8;
            s.setPixelColor(i, s.Color(r, g, b));
        };
        fade(stripLeft);
        fade(stripRight);
    }
}

// ---------------------------------------------------------------------------
// Brake lights (plain GPIO)
// ---------------------------------------------------------------------------

void EyeMatrix::brakeOn() {
    digitalWrite(BRAKE_LED_L, HIGH);
    digitalWrite(BRAKE_LED_R, HIGH);
}

void EyeMatrix::brakeOff() {
    digitalWrite(BRAKE_LED_L, LOW);
    digitalWrite(BRAKE_LED_R, LOW);
}

// Brake fires when decelerating from forward motion or reversing
void EyeMatrix::brakeUpdate(int motorFwd) {
    bool decelerating = (prevMotorFwd > 30 && motorFwd < prevMotorFwd - 10);
    bool reversing    = (motorFwd < -20);

    if ((decelerating || reversing) && !brakeActive) {
        brakeActive   = true;
        brakeStartMs  = millis();
    }

    if (brakeActive) {
        bool stillBraking = decelerating || reversing || (millis() - brakeStartMs < 400);
        if (stillBraking) {
            brakeOn();
        } else {
            brakeActive = false;
            brakeOff();
        }
    } else {
        brakeOff();
    }

    prevMotorFwd = motorFwd;
}

// ---------------------------------------------------------------------------
// Main update
// ---------------------------------------------------------------------------

void EyeMatrix::update(int mode, int motorFwd, int motorTurn) {
    if (mode != currentMode) {
        currentMode = mode;
        state.reset();
        fillBoth(0);
    }

    switch (currentMode) {
        case 0: modeHeadlights();        break;
        case 1: modeTurboSweep();        break;
        case 2: modeLavaFlow();          break;
        case 3: modeNeonUnderglow();     break;
        case 4: modePoliceInterceptor(); break;
        case 5: modePlasmaWave();        break;
        case 6: modeSynthwaveHorizon();  break;
        case 7: modeCometTrail();        break;
        case 8: modeEmberFlicker();      break;
        case 9: modeQuantumSparkle();    break;
        default: modeHeadlights();       break;
    }

    brakeUpdate(motorFwd);
    show();
}

// ---------------------------------------------------------------------------
// Mode 0 – Headlights
// Left strip: cool white headlights
// Right strip: warm amber tail/running lights
// Static — only draws once per mode entry
// ---------------------------------------------------------------------------
void EyeMatrix::modeHeadlights() {
    if (state.step == 0) {
        for (int i = 0; i < NUM_LEDS; i++) {
            stripLeft.setPixelColor(i,  stripLeft.Color(255, 255, 210)); // cool white
            stripRight.setPixelColor(i, stripRight.Color(200, 80, 0));   // amber
        }
        state.step = 1;
    }
}

// ---------------------------------------------------------------------------
// Mode 1 – Turbo Sweep
// A bright white comet launches from the inner end of each strip outward
// simultaneously, leaving a short blue trail — like a turbo boost flash.
// ---------------------------------------------------------------------------
void EyeMatrix::modeTurboSweep() {
    fadeAccent(80);
    unsigned long t = millis();
    int period = NUM_LEDS * 2;
    int raw = (t / 30) % period;
    int pos = (raw < NUM_LEDS) ? raw : (period - 1 - raw);

    // Left strip: sweeps 0→9 (inner to outer)
    stripLeft.setPixelColor(pos, stripLeft.Color(255, 255, 255));
    if (pos > 0) stripLeft.setPixelColor(pos - 1, stripLeft.Color(60, 60, 255));

    // Right strip: sweeps 9→0 (mirrored, both go outward from center)
    int rpos = NUM_LEDS - 1 - pos;
    stripRight.setPixelColor(rpos, stripRight.Color(255, 255, 255));
    if (rpos < NUM_LEDS - 1) stripRight.setPixelColor(rpos + 1, stripRight.Color(60, 60, 255));
}

// ---------------------------------------------------------------------------
// Mode 2 – Lava Flow
// Slow molten gradient drifts along both strips.
// Colours cycle through deep red → orange → yellow hot-spot → back.
// ---------------------------------------------------------------------------
void EyeMatrix::modeLavaFlow() {
    unsigned long t = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
        // Offset each pixel in time to create a flowing wave
        float phase = (t / 1200.0f) + (i * 0.35f);
        float heat  = (sinf(phase) + 1.0f) / 2.0f; // 0..1

        uint8_t r = 255;
        uint8_t g = (uint8_t)(heat * heat * 160);   // non-linear: mostly dark orange
        uint8_t b = 0;

        uint32_t c = stripLeft.Color(r, g, b);
        stripLeft.setPixelColor(i, c);
        stripRight.setPixelColor(NUM_LEDS - 1 - i, c); // mirror flow direction
    }
    delay(15);
}

// ---------------------------------------------------------------------------
// Mode 3 – Neon Underglow
// Left strip locked to vivid cyan, right strip to electric lime.
// Both slowly breathe in and out together for a clean underglow look.
// ---------------------------------------------------------------------------
void EyeMatrix::modeNeonUnderglow() {
    unsigned long t = millis();
    float breath = (sinf(t / 900.0f) + 1.0f) / 2.0f;
    uint8_t v = (uint8_t)(40 + breath * 215); // never fully off

    for (int i = 0; i < NUM_LEDS; i++) {
        stripLeft.setPixelColor(i,  stripLeft.Color(0, v, v));       // cyan
        stripRight.setPixelColor(i, stripRight.Color(v / 2, v, 0));  // lime
    }
    delay(15);
}

// ---------------------------------------------------------------------------
// Mode 4 – Police Interceptor
// Both strips strobe red and blue in rapid alternation.
// Left = red phase, right = blue phase, swap every 80 ms.
// ---------------------------------------------------------------------------
void EyeMatrix::modePoliceInterceptor() {
    fillBoth(0);
    unsigned long t = millis();
    bool phase = (t / 80) % 2;

    uint32_t red  = stripLeft.Color(255, 0, 0);
    uint32_t blue = stripLeft.Color(0, 0, 255);

    for (int i = 0; i < NUM_LEDS; i++) {
        stripLeft.setPixelColor(i,  phase ? red  : 0);
        stripRight.setPixelColor(i, phase ? 0    : blue);
    }
}

// ---------------------------------------------------------------------------
// Mode 5 – Plasma Wave
// Two sine waves of different hues travel in opposite directions and
// interfere — producing a shimmering, ever-changing colour ripple.
// ---------------------------------------------------------------------------
void EyeMatrix::modePlasmaWave() {
    unsigned long t = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
        float w1 = sinf(t / 400.0f + i * 0.6f);
        float w2 = sinf(t / 250.0f - i * 0.9f);
        float combined = (w1 + w2 + 2.0f) / 4.0f; // 0..1

        uint16_t hue = (uint16_t)(combined * 65535);
        uint32_t c = stripLeft.gamma32(stripLeft.ColorHSV(hue, 255, 255));
        stripLeft.setPixelColor(i, c);
        stripRight.setPixelColor(NUM_LEDS - 1 - i, c);
    }
    delay(10);
}

// ---------------------------------------------------------------------------
// Mode 6 – Synthwave Horizon
// A pink→purple→deep-blue gradient slowly rotates its hue offset,
// giving the feel of a neon sunset sliding across the car.
// ---------------------------------------------------------------------------
void EyeMatrix::modeSynthwaveHorizon() {
    unsigned long t = millis();
    uint16_t baseHue = (uint16_t)((t / 25) % 65536); // slow global rotation

    for (int i = 0; i < NUM_LEDS; i++) {
        // Spread ~8000 hue units across the strip (pink→purple band)
        uint16_t hue = baseHue + (uint16_t)(i * 800);
        uint32_t c = stripLeft.gamma32(stripLeft.ColorHSV(hue, 240, 255));
        stripLeft.setPixelColor(i, c);
        stripRight.setPixelColor(NUM_LEDS - 1 - i, c); // mirror
    }
    delay(10);
}

// ---------------------------------------------------------------------------
// Mode 7 – Comet Trail
// A bright white comet bounces end-to-end on both strips with a long
// glowing cyan tail that fades out behind it.
// ---------------------------------------------------------------------------
void EyeMatrix::modeCometTrail() {
    fadeAccent(45); // long tail
    unsigned long t = millis();
    int period = (NUM_LEDS - 1) * 2;
    int raw = (t / 25) % period;
    int pos = (raw < NUM_LEDS) ? raw : (period - raw);

    // Head: white
    stripLeft.setPixelColor(pos,              stripLeft.Color(255, 255, 255));
    stripRight.setPixelColor(NUM_LEDS-1-pos,  stripRight.Color(255, 255, 255));

    // Immediate tail pixel: bright cyan
    int tail = pos - 1;
    if (tail >= 0) {
        stripLeft.setPixelColor(tail,             stripLeft.Color(0, 200, 255));
        stripRight.setPixelColor(NUM_LEDS-1-tail, stripRight.Color(0, 200, 255));
    }
}

// ---------------------------------------------------------------------------
// Mode 8 – Ember Flicker
// Each pixel independently flickers with a heat-map colour:
// white-hot core → orange → deep red → black, randomised per pixel.
// ---------------------------------------------------------------------------
void EyeMatrix::modeEmberFlicker() {
    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t heat = random(60, 255);
        uint8_t r, g, b;
        if (heat > 200) {
            // White hot
            r = 255; g = 255; b = (heat - 200) * 5;
        } else if (heat > 120) {
            // Orange
            r = 255; g = map(heat, 120, 200, 40, 180); b = 0;
        } else {
            // Deep red
            r = map(heat, 60, 120, 60, 255); g = 0; b = 0;
        }
        stripLeft.setPixelColor(i,  stripLeft.Color(r, g, b));
        stripRight.setPixelColor(i, stripRight.Color(r, g, b));
    }
    delay(25);
}

// ---------------------------------------------------------------------------
// Mode 9 – Quantum Sparkle
// Deep black background. Random single pixels pop to a vivid random hue
// at full brightness, then decay independently — like stars igniting.
// ---------------------------------------------------------------------------
void EyeMatrix::modeQuantumSparkle() {
    fadeAccent(18); // slow decay — long glowing tail per spark

    // Spawn ~2 new sparks per frame across both strips
    for (int s = 0; s < 2; s++) {
        if (random(0, 100) < 40) {
            int p = random(0, NUM_LEDS);
            uint32_t c = stripLeft.gamma32(
                stripLeft.ColorHSV(random(0, 65535), 255, 255));
            stripLeft.setPixelColor(p, c);
        }
        if (random(0, 100) < 40) {
            int p = random(0, NUM_LEDS);
            uint32_t c = stripRight.gamma32(
                stripRight.ColorHSV(random(0, 65535), 255, 255));
            stripRight.setPixelColor(p, c);
        }
    }
    delay(12);
}

// ---------------------------------------------------------------------------
// Signal loss – slow red breathing on both strips, brake LEDs off
// ---------------------------------------------------------------------------
void EyeMatrix::displaySignalLoss() {
    float val = (sinf(millis() / 300.0f) + 1.0f) / 2.0f;
    uint8_t r = (uint8_t)(val * 255);
    fillBoth(stripLeft.Color(r, 0, 0));
    brakeOff();
    show();
}
