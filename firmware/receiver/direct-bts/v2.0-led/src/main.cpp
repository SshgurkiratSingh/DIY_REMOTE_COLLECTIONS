/**
 * ============================================================================
 *  ESP-NOW Receiver v2.0-led (EYE MATRIX EDITION)
 *  L298N Motor Driver + Dual WS2812B LED Strips (10 LEDs each)
 * ============================================================================
 *
 *  Compatible with: Transmitter v2.0 (ESP-NOW OLED Remote)
 *  Board:           ESP32 DevKit
 *  Motor Driver:    L298N (dual H-bridge)
 *  LED Strip:       2x WS2812B (10 LEDs per eye, GRB order)
 *
 *  Eye Layout (physical):
 *    Line 1  : LEDs 0, 1, 2
 *    Hidden  : LED  3        ← always off (routing LED, never visible)
 *    Line 2  : LEDs 4, 5, 6
 *    Hidden  : LED  7        ← always off (routing LED, never visible)
 *    Line 3  : LEDs 8, 9
 *
 *  Controls (from transmitter v2.0):
 *    joyY        → Throttle (forward / backward)
 *    joyX        → Steering (left / right)
 *    potValue    → LED brightness (0-4095 → 10-255, floor at 10 so strip isn't dark)
 *    toggle1     → Motor-LED sync mode ON/OFF
 *    toggle2     → Motor enable / disable (safety kill switch)
 *    addrLedMode → LED mode (0-9) set from remote Settings menu
 *
 * ============================================================================
 *  BUGS FIXED vs previous version
 *  1. eyeSolidHeadlights() ran every frame with no guard → overwrote all modes
 *  2. animStep was unsigned long → signed-bounce math broke (never reversed)
 *  3. Deadzone was applied after map() → 7-unit deadzone on 255 scale was ok
 *     but raw ADC deadzone is now applied correctly around joystick center
 *  4. struct_message now has __attribute__((packed)) on both sides to guarantee
 *     identical wire layout and prevent silent packet drops
 * ============================================================================
 */

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <FastLED.h>
#include <math.h>

// ============================================================================
//  Pin Definitions
// ============================================================================

// L298N Motor Driver
#define ENA_PIN        25
#define IN1_PIN        26
#define IN2_PIN        27
#define ENB_PIN        14
#define IN3_PIN        12
#define IN4_PIN        13

// WS2812B LED Strips (Left eye / Right eye)
#define LED_PIN_LEFT   15
#define LED_PIN_RIGHT  16
#define NUM_LEDS       10
#define LED_TYPE       WS2812B
#define COLOR_ORDER    GRB

// Status LED (onboard)
#define STATUS_LED     2

// Battery voltage divider (optional, ADC1)
#define BATT_ADC_PIN   34

// ============================================================================
//  Configuration
// ============================================================================

// Motor PWM (ESP32 LEDC)
#define PWM_FREQ         5000
#define PWM_RES_BITS     8
#define PWM_CHANNEL_A    0
#define PWM_CHANNEL_B    1

// Joystick
#define JOY_MAX          4095
#define JOY_CENTER       2048
// Raw-ADC deadzone around center.  ±150 counts ≈ 7% of range.
#define JOY_DEADZONE_RAW 150

// Speed ramping (units per loop tick at ~100 Hz)
#define RAMP_STEP        12

// Safety
#define SIGNAL_TIMEOUT_MS   1000
#define FEEDBACK_INTERVAL_MS 500

// LED
#define MIN_BRIGHTNESS    10      // Never go fully dark while connected
#define DEFAULT_BRIGHTNESS 150
#define TOTAL_LED_MODES   10

// Packet auth (0 = accept any transmitter)
#define EXPECTED_VERIFY_KEY 0

// ============================================================================
//  Data Structures   — MUST match transmitter v2.0 Config.h exactly
// ============================================================================

typedef struct __attribute__((packed)) struct_message {
    uint16_t joyX;
    uint16_t joyY;
    uint16_t potValue;
    bool     toggle1;
    bool     toggle2;
    bool     push1;
    bool     push2;
    uint8_t  verifyKey;
    uint8_t  addrLedMode;
} struct_message;

typedef struct __attribute__((packed)) rx_message {
    uint8_t data1;   // active LED mode
    uint8_t data2;   // motor-sync flag
    uint8_t data3;   // battery (0-255)
} rx_message;

// ============================================================================
//  Global State
// ============================================================================

// ---- ESP-NOW ----
volatile struct_message incomingData;
rx_message              feedbackData;
unsigned long           lastRecvTime     = 0;
uint8_t                 senderMac[6]     = {0};
bool                    hasSender        = false;

// ---- Motors ----
int   targetSpeedA  = 0, currentSpeedA = 0;
int   targetSpeedB  = 0, currentSpeedB = 0;
int   prevThrottle  = 0;

// ---- LEDs ----
CRGB  ledsLeft[NUM_LEDS];
CRGB  ledsRight[NUM_LEDS];
int   ledMode             = 0;
int   lastLedMode         = -1;   // track mode changes for one-shot animations
uint8_t ledBrightness     = DEFAULT_BRIGHTNESS;
bool  motorSyncEnabled    = false;

// ---- Animation state ----
// FIX: use signed int for step counters that bounce negative
unsigned long animTimer   = 0;
unsigned long animStep    = 0;
int           scanPos     = 0;    // signed scan position for eyeScanning
int           scanDir     = 1;
uint8_t       animHue     = 0;

// ---- Feedback ----
unsigned long lastFeedbackTime = 0;

// ============================================================================
//  Motor Control (L298N)
// ============================================================================

void initMotors() {
    pinMode(IN1_PIN, OUTPUT);  pinMode(IN2_PIN, OUTPUT);
    pinMode(IN3_PIN, OUTPUT);  pinMode(IN4_PIN, OUTPUT);
    ledcSetup(PWM_CHANNEL_A, PWM_FREQ, PWM_RES_BITS);
    ledcSetup(PWM_CHANNEL_B, PWM_FREQ, PWM_RES_BITS);
    ledcAttachPin(ENA_PIN, PWM_CHANNEL_A);
    ledcAttachPin(ENB_PIN, PWM_CHANNEL_B);
}

void setMotorA(int speed) {
    speed = constrain(speed, -255, 255);
    if      (speed > 0) { digitalWrite(IN1_PIN, HIGH); digitalWrite(IN2_PIN, LOW);  ledcWrite(PWM_CHANNEL_A, speed);  }
    else if (speed < 0) { digitalWrite(IN1_PIN, LOW);  digitalWrite(IN2_PIN, HIGH); ledcWrite(PWM_CHANNEL_A, -speed); }
    else                { digitalWrite(IN1_PIN, LOW);  digitalWrite(IN2_PIN, LOW);  ledcWrite(PWM_CHANNEL_A, 0);      }
}

void setMotorB(int speed) {
    speed = constrain(speed, -255, 255);
    if      (speed > 0) { digitalWrite(IN3_PIN, HIGH); digitalWrite(IN4_PIN, LOW);  ledcWrite(PWM_CHANNEL_B, speed);  }
    else if (speed < 0) { digitalWrite(IN3_PIN, LOW);  digitalWrite(IN4_PIN, HIGH); ledcWrite(PWM_CHANNEL_B, -speed); }
    else                { digitalWrite(IN3_PIN, LOW);  digitalWrite(IN4_PIN, LOW);  ledcWrite(PWM_CHANNEL_B, 0);      }
}

void stopMotors() {
    setMotorA(0); setMotorB(0);
    currentSpeedA = targetSpeedA = 0;
    currentSpeedB = targetSpeedB = 0;
}

int rampTowards(int cur, int tgt, int step) {
    if (cur < tgt) return min(cur + step, tgt);
    if (cur > tgt) return max(cur - step, tgt);
    return cur;
}

void mixDrive(uint16_t throttleRaw, uint16_t steerRaw) {
    // FIX: apply deadzone to raw ADC value BEFORE mapping
    int tRaw = (int)throttleRaw;
    int sRaw = (int)steerRaw;
    if (abs(tRaw - JOY_CENTER) < JOY_DEADZONE_RAW) tRaw = JOY_CENTER;
    if (abs(sRaw - JOY_CENTER) < JOY_DEADZONE_RAW) sRaw = JOY_CENTER;

    int throttle = map(tRaw, 0, JOY_MAX, -255, 255);
    int steer    = map(sRaw, 0, JOY_MAX, -255, 255);

    targetSpeedA = constrain(throttle + steer, -255, 255);
    targetSpeedB = constrain(throttle - steer, -255, 255);
}

void updateMotors() {
    currentSpeedA = rampTowards(currentSpeedA, targetSpeedA, RAMP_STEP);
    currentSpeedB = rampTowards(currentSpeedB, targetSpeedB, RAMP_STEP);
    setMotorA(currentSpeedA);
    setMotorB(currentSpeedB);
}

// ============================================================================
//  Eye Matrix LED Helpers
// ============================================================================

void fillBoth(CRGB color) {
    fill_solid(ledsLeft, NUM_LEDS, color);
    fill_solid(ledsRight, NUM_LEDS, color);
}

void fadeBoth(uint8_t amount) {
    for (int i = 0; i < NUM_LEDS; i++) {
        ledsLeft[i].fadeToBlackBy(amount);
        ledsRight[i].fadeToBlackBy(amount);
    }
}

// Always applied last, just before FastLED.show()
void maskHiddenLEDs() {
    ledsLeft[3] = ledsRight[3] = CRGB::Black;
    ledsLeft[7] = ledsRight[7] = CRGB::Black;
}

// Reset all animation counters when mode changes
void resetAnim() {
    animTimer  = 0;
    animStep   = 0;
    scanPos    = 0;
    scanDir    = 1;
    animHue    = 0;
}

// ============================================================================
//  10 Custom Eye Animations  (all non-blocking via millis() guards)
// ============================================================================

// Mode 0: Solid Headlights — FIX: dirty flag so it writes only once
void eyeSolidHeadlights() {
    if (lastLedMode == ledMode) return; // already painted, skip
    fillBoth(CRGB::White);
}

// Mode 1: Angry Eyes — also a static pattern, use dirty flag
void eyeAngry() {
    if (lastLedMode == ledMode) return;
    fillBoth(CRGB::Black);
    for (int i = 0; i < 3; i++) {
        ledsLeft[i] = ledsRight[i] = CRGB::Red;
    }
    for (int i = 4; i < 7; i++) {
        ledsLeft[i] = ledsRight[i] = CRGB::Red;
    }
}

// Mode 2: Scanning Pupil — FIX: use signed scanPos/scanDir
void eyeScanning() {
    if (millis() - animTimer < 120) return;
    animTimer = millis();

    fadeBoth(160);

    // scanPos oscillates 0 → 1 → 2 → 1 → 0 (3 columns)
    int col  = scanPos;
    int led1 = col;            // Line 1: idx 0,1,2
    int led2 = 4 + col;        // Line 2: idx 4,5,6
    // Line 3 only has indices 8,9.  Map col 0→none, 1→8, 2→9
    int led3 = (col == 1) ? 8 : (col == 2 ? 9 : -1);

    ledsLeft[led1] = ledsRight[led1] = CRGB::Red;
    ledsLeft[led2] = ledsRight[led2] = CRGB::Red;
    if (led3 >= 0) ledsLeft[led3] = ledsRight[led3] = CRGB::Red;

    scanPos += scanDir;
    if (scanPos >= 2) scanDir = -1;
    if (scanPos <= 0) scanDir =  1;
}

// Mode 3: Natural Blinking (solid white, ~150 ms blink every 3 s)
void eyeBlinking() {
    uint32_t t     = millis();
    uint32_t cycle = (t / 150) % 20;   // period = 3000 ms / 150 ms = 20 slots
    if (cycle == 0) {
        fillBoth(CRGB::Black);
    } else {
        if (lastLedMode != ledMode || cycle == 1) // repaint after blink closes
            fillBoth(CRGB::White);
    }
}

// Mode 4: Sleepy Breathing — lower lines only, cyan sine-wave
void eyeSleepyBreathing() {
    if (millis() - animTimer < 20) return;
    animTimer = millis();
    animStep++;

    fillBoth(CRGB::Black);
    uint8_t val = (uint8_t)((sinf(animStep * 0.03f) + 1.0f) * 100.0f + 30.0f);
    for (int i = 4; i < 7;  i++) ledsLeft[i] = ledsRight[i] = CHSV(160, 255, val);
    for (int i = 8; i < 10; i++) ledsLeft[i] = ledsRight[i] = CHSV(160, 255, val);
}

// Mode 5: Rainbow Flow — hue shifts across rows
void eyeRainbow() {
    if (millis() - animTimer < 30) return;
    animTimer = millis();
    animHue++;
    for (int i = 0; i < NUM_LEDS; i++) {
        CRGB color = CHSV(animHue + (uint8_t)(i * 20), 255, 255);
        ledsLeft[i] = ledsRight[i] = color;
    }
}

// Mode 6: Hypnotic Lines — cycles through row 1 → 2 → 3 with color change
void eyeHypnotic() {
    if (millis() - animTimer < 100) return;
    animTimer = millis();
    animStep++;

    fadeBoth(80);
    int   phase = animStep % 3;
    CRGB  color = CHSV((uint8_t)(animStep * 10), 255, 255);

    if (phase == 0)      for (int i = 0; i < 3;  i++) ledsLeft[i] = ledsRight[i] = color;
    else if (phase == 1) for (int i = 4; i < 7;  i++) ledsLeft[i] = ledsRight[i] = color;
    else                 for (int i = 8; i < 10; i++) ledsLeft[i] = ledsRight[i] = color;
}

// Mode 7: Fire Flicker
void eyeFire() {
    if (millis() - animTimer < 50) return;
    animTimer = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
        ledsLeft[i]  = CHSV(random8(0, 40), 255, random8(100, 255));
        ledsRight[i] = CHSV(random8(0, 40), 255, random8(100, 255));
    }
}

// Mode 8: Police Strobe — left eye red, right eye blue, alternating
void eyePoliceStrobe() {
    if (millis() - animTimer < 100) return;
    animTimer = millis();
    animStep++;

    int phase = (animStep / 2) % 4;
    switch (phase) {
        case 0: fill_solid(ledsLeft,  NUM_LEDS, CRGB::Red);  fill_solid(ledsRight, NUM_LEDS, CRGB::Black); break;
        case 1: fillBoth(CRGB::Black);  break;
        case 2: fill_solid(ledsLeft,  NUM_LEDS, CRGB::Black); fill_solid(ledsRight, NUM_LEDS, CRGB::Blue); break;
        case 3: fillBoth(CRGB::Black);  break;
    }
}

// Mode 9: Cyber Sparkle — random hue sparks on both eyes independently
void eyeSparkle() {
    if (millis() - animTimer < 40) return;
    animTimer = millis();
    fadeBoth(60);
    if (random8() < 120) ledsLeft [random8(NUM_LEDS)] = CHSV(random8(), 200, 255);
    if (random8() < 120) ledsRight[random8(NUM_LEDS)] = CHSV(random8(), 200, 255);
}

// ============================================================================
//  Motor-LED Sync Mode
// ============================================================================

void ledMotorSync(uint16_t throttleRaw, uint16_t steerRaw) {
    // Apply deadzone on raw value first
    int tRaw = (int)throttleRaw;
    int sRaw = (int)steerRaw;
    if (abs(tRaw - JOY_CENTER) < JOY_DEADZONE_RAW) tRaw = JOY_CENTER;
    if (abs(sRaw - JOY_CENTER) < JOY_DEADZONE_RAW) sRaw = JOY_CENTER;

    int fwd  = map(tRaw, 0, JOY_MAX, -255, 255);
    int turn = map(sRaw, 0, JOY_MAX, -255, 255);

    int absFwd  = abs(fwd);
    int absTurn = abs(turn);
    int speed   = max(absFwd, absTurn);
    bool braking = (prevThrottle > 100 && absFwd < 30);

    if (braking) {
        if (millis() - animTimer < 50) return;
        animTimer = millis(); animStep++;
        fillBoth((animStep % 2 == 0) ? CRGB::Red : CRGB::Black);
    } else if (speed > 220) {
        if (millis() - animTimer < 15) return;
        animTimer = millis(); animHue += 5;
        for (int i = 0; i < NUM_LEDS; i++)
            ledsLeft[i] = ledsRight[i] = CHSV(animHue + (uint8_t)(i * 25), 255, 255);
    } else if (fwd != 0) {
        unsigned long interval = (unsigned long)map(absFwd, 30, 255, 100, 20);
        if (millis() - animTimer < interval) return;
        animTimer = millis(); animStep++;
        fadeBoth(100);
        CRGB driveColor = (fwd > 0) ? CRGB::Green : CRGB::Red;
        ledsLeft [random8(NUM_LEDS)] = driveColor;
        ledsRight[random8(NUM_LEDS)] = driveColor;
        if (absTurn > 30) {
            if (turn > 0) fill_solid(ledsRight, NUM_LEDS, CRGB::Orange);
            else          fill_solid(ledsLeft,  NUM_LEDS, CRGB::Orange);
        }
    } else if (absTurn > 30) {
        if (millis() - animTimer < 150) return;
        animTimer = millis(); animStep++;
        fillBoth(CRGB::Black);
        if (animStep % 2 == 0) {
            uint8_t bright = (uint8_t)map(absTurn, 30, 255, 100, 255);
            CRGB turnColor = CHSV(25, 255, bright);
            if (turn > 0) fill_solid(ledsRight, NUM_LEDS, turnColor);
            else          fill_solid(ledsLeft,  NUM_LEDS, turnColor);
        }
    } else {
        // Idle: gentle green breathing
        if (millis() - animTimer < 20) return;
        animTimer = millis(); animStep++;
        uint8_t val = (uint8_t)((sinf(animStep * 0.04f) + 1.0f) * 100.0f + 20.0f);
        fillBoth(CHSV(96, 255, val));
    }
    prevThrottle = absFwd;
}

// ============================================================================
//  LED Mode Dispatcher
// ============================================================================

void updateLEDs() {
    if (motorSyncEnabled) {
        ledMotorSync(incomingData.joyY, incomingData.joyX);
    } else {
        switch (ledMode) {
            case 0: eyeSolidHeadlights(); break;
            case 1: eyeAngry();           break;
            case 2: eyeScanning();        break;
            case 3: eyeBlinking();        break;
            case 4: eyeSleepyBreathing(); break;
            case 5: eyeRainbow();         break;
            case 6: eyeHypnotic();        break;
            case 7: eyeFire();            break;
            case 8: eyePoliceStrobe();    break;
            case 9: eyeSparkle();         break;
            default: eyeSolidHeadlights(); break;
        }
    }

    lastLedMode = ledMode;   // update dirty-flag tracker AFTER rendering
    maskHiddenLEDs();
    FastLED.show();
}

// ============================================================================
//  Input Handling
// ============================================================================

void handleInputs() {
    // --- LED mode sync ---
    int newMode = incomingData.addrLedMode;
    if (newMode >= TOTAL_LED_MODES) newMode = 0;
    if (newMode != ledMode) {
        ledMode = newMode;
        resetAnim();
        Serial.printf("LED Mode → %d\n", ledMode);
    }

    // --- Toggle1: motor-sync ---
    motorSyncEnabled = incomingData.toggle1;
}

// ============================================================================
//  ESP-NOW Callbacks
// ============================================================================

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (!info || !info->src_addr) return;
    const uint8_t *mac = info->src_addr;
#else
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
#endif
    if ((size_t)len != sizeof(struct_message)) {
        Serial.printf("RX size mismatch: got %d, expected %d\n", len, sizeof(struct_message));
        return;
    }

    struct_message tmp;
    memcpy(&tmp, data, sizeof(tmp));

    if (EXPECTED_VERIFY_KEY != 0 && tmp.verifyKey != EXPECTED_VERIFY_KEY) return;

    // Safe copy (ISR context)
    memcpy((void*)&incomingData, &tmp, sizeof(tmp));
    lastRecvTime = millis();

    if (!hasSender) {
        memcpy(senderMac, mac, 6);
        hasSender = true;

        esp_now_peer_info_t peer = {};
        memcpy(peer.peer_addr, senderMac, 6);
        peer.channel = 0;
        peer.encrypt = false;
        if (!esp_now_is_peer_exist(senderMac)) esp_now_add_peer(&peer);

        char buf[18];
        snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
        Serial.printf("Paired with: %s\n", buf);
    }
}

// ============================================================================
//  Feedback to Transmitter
// ============================================================================

void sendFeedback() {
    if (!hasSender) return;
    if (millis() - lastFeedbackTime < FEEDBACK_INTERVAL_MS) return;
    lastFeedbackTime = millis();

    feedbackData.data1 = (uint8_t)ledMode;
    feedbackData.data2 = motorSyncEnabled ? 1 : 0;
    feedbackData.data3 = (uint8_t)map(analogRead(BATT_ADC_PIN), 0, 4095, 0, 255);
    esp_now_send(senderMac, (uint8_t*)&feedbackData, sizeof(feedbackData));
}

// ============================================================================
//  Setup
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(300);

    Serial.println("====================================");
    Serial.printf("  struct_message size: %d bytes\n", sizeof(struct_message));
    Serial.println("  ESP-NOW Receiver v2.0-led");
    Serial.println("  L298N + Dual WS2812B Eye Matrix");
    Serial.println("====================================");

    initMotors();
    stopMotors();

    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(STATUS_LED, LOW);

    FastLED.addLeds<LED_TYPE, LED_PIN_LEFT,  COLOR_ORDER>(ledsLeft,  NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.addLeds<LED_TYPE, LED_PIN_RIGHT, COLOR_ORDER>(ledsRight, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(DEFAULT_BRIGHTNESS);
    fillBoth(CRGB::Black);
    maskHiddenLEDs();
    FastLED.show();

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    Serial.printf("Receiver MAC: %s\n", WiFi.macAddress().c_str());

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init FAILED!");
        return;
    }
    esp_now_register_recv_cb(onDataRecv);

    // Safe neutral defaults
    memset((void*)&incomingData, 0, sizeof(incomingData));
    incomingData.joyX     = JOY_CENTER;
    incomingData.joyY     = JOY_CENTER;

    // Startup rainbow sweep
    for (int h = 0; h < 256; h += 6) {
        fillBoth(CHSV(h, 255, 200));
        maskHiddenLEDs();
        FastLED.show();
        delay(8);
    }
    fillBoth(CRGB::Black);
    maskHiddenLEDs();
    FastLED.show();

    Serial.println("Ready. Waiting for transmitter...");
}

// ============================================================================
//  Main Loop
// ============================================================================

void loop() {
    unsigned long now = millis();

    // ── Safety timeout ────────────────────────────────────────────────────────
    if (now - lastRecvTime > SIGNAL_TIMEOUT_MS) {
        stopMotors();
        digitalWrite(STATUS_LED, LOW);

        static unsigned long lostTimer = 0;
        static unsigned long lostStep  = 0;
        if (now - lostTimer > 30) {
            lostTimer = now;
            lostStep++;
            uint8_t val = (uint8_t)((sinf(lostStep * 0.06f) + 1.0f) * 80.0f);
            fillBoth(CRGB(val, 0, 0));
            maskHiddenLEDs();
            FastLED.show();
        }
        delay(10);
        return;
    }

    // ── Active ────────────────────────────────────────────────────────────────
    digitalWrite(STATUS_LED, HIGH);

    // Brightness from pot — floor at MIN_BRIGHTNESS so strip is always visible
    ledBrightness = (uint8_t)max((int)MIN_BRIGHTNESS,
                                 (int)map(incomingData.potValue, 0, 4095, 0, 255));
    FastLED.setBrightness(ledBrightness);

    handleInputs();

    // Motors — toggle2 must be HIGH to enable
    if (incomingData.toggle2) {
        mixDrive(incomingData.joyY, incomingData.joyX);
        updateMotors();
    } else {
        stopMotors();
    }

    updateLEDs();
    sendFeedback();

    delay(10);  // ~100 Hz
}
