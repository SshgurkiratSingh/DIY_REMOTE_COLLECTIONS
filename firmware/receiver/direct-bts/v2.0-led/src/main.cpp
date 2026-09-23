/**
 * ============================================================================
 *  ESP-NOW Receiver v2.0-led
 *  L298N Motor Driver + Dual WS2812B LED Strips (10 LEDs each)
 * ============================================================================
 *
 *  Compatible with: Transmitter v2.0 (ESP-NOW OLED Remote)
 *  Board:           ESP32 DevKit
 *  Motor Driver:    L298N (dual H-bridge)
 *  LED Strip:       2x WS2812B / WS2811 / SK6812 (10 LEDs each for Left/Right)
 *
 *  Controls:
 *    joyY       → Throttle (forward/backward)
 *    joyX       → Steering (left/right)
 *    potValue   → LED brightness (0-4095 → 0-255)
 *    push1      → Cycle LED mode forward  (edge-triggered)
 *    push2      → Cycle LED mode backward  (edge-triggered)
 *    toggle1    → Motor-LED sync mode ON/OFF
 *    toggle2    → Motor enable/disable (safety kill switch)
 *    verifyKey  → Pairing validation (0 = accept all)
 *
 * ============================================================================
 */

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <FastLED.h>

// ============================================================================
//  Pin Definitions
// ============================================================================

// L298N Motor Driver
#define ENA_PIN   25   // Motor A enable (PWM)
#define IN1_PIN   26   // Motor A direction 1
#define IN2_PIN   27   // Motor A direction 2
#define ENB_PIN   14   // Motor B enable (PWM)
#define IN3_PIN   12   // Motor B direction 1
#define IN4_PIN   13   // Motor B direction 2

// WS2812B LED Strips (Left and Right)
#define LED_PIN_LEFT   15
#define LED_PIN_RIGHT  16
#define NUM_LEDS       10
#define LED_TYPE       WS2812B
#define COLOR_ORDER    GRB

// Status LED
#define STATUS_LED     2

// Battery ADC
#define BATT_ADC_PIN   34

// ============================================================================
//  Configuration
// ============================================================================

// Motor PWM
#define PWM_FREQ       5000
#define PWM_RESOLUTION 8
#define PWM_CHANNEL_A  0
#define PWM_CHANNEL_B  1

// Joystick
#define JOY_MAX        4095
#define JOY_CENTER     2048
#define JOY_DEADZONE   120

// Speed ramping
#define RAMP_STEP      15

// Safety
#define SIGNAL_TIMEOUT 1000  // ms before safety stop
#define FEEDBACK_INTERVAL 500 // ms between feedback packets

// LED
#define DEFAULT_BRIGHTNESS 128
#define TOTAL_LED_MODES    12

// Pairing (0 = accept any key)
#define EXPECTED_VERIFY_KEY 0

// ============================================================================
//  Data Structures (must match transmitter v2.0)
// ============================================================================

typedef struct struct_message {
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

// Feedback to transmitter (rx_message, 3 bytes)
typedef struct rx_message {
    uint8_t data1;   // Current LED mode (0-11)
    uint8_t data2;   // Motor-LED sync state (0 or 1)
    uint8_t data3;   // Battery/status byte
} rx_message;

// ============================================================================
//  Global State
// ============================================================================

// ESP-NOW
struct_message incomingData;
rx_message     feedbackData;
unsigned long  lastRecvTime = 0;
uint8_t        senderMac[6];
bool           hasSender = false;

// Motor state
int   targetSpeedA   = 0;
int   targetSpeedB   = 0;
int   currentSpeedA  = 0;
int   currentSpeedB  = 0;
int   prevThrottle   = 0;  // For braking detection

// LED state
CRGB  ledsLeft[NUM_LEDS];
CRGB  ledsRight[NUM_LEDS];
int   ledMode        = 0;
uint8_t ledBrightness = DEFAULT_BRIGHTNESS;
bool  motorSyncEnabled = false;

// Button edge detection
bool  lastPush1      = false;
bool  lastPush2      = false;

// Animation timing (non-blocking)
unsigned long animTimer      = 0;
unsigned long animStep       = 0;
int           animDirection  = 1;   // For Knight Rider bounce
uint8_t       animHue        = 0;   // Global rotating hue

// Feedback timing
unsigned long lastFeedbackTime = 0;

// ============================================================================
//  Motor Control (L298N)
// ============================================================================

void initMotors() {
    // Direction pins
    pinMode(IN1_PIN, OUTPUT);
    pinMode(IN2_PIN, OUTPUT);
    pinMode(IN3_PIN, OUTPUT);
    pinMode(IN4_PIN, OUTPUT);

    // PWM for enable pins
    ledcSetup(PWM_CHANNEL_A, PWM_FREQ, PWM_RESOLUTION);
    ledcSetup(PWM_CHANNEL_B, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(ENA_PIN, PWM_CHANNEL_A);
    ledcAttachPin(ENB_PIN, PWM_CHANNEL_B);
}

void setMotorA(int speed) {
    speed = constrain(speed, -255, 255);
    if (speed > 0) {
        digitalWrite(IN1_PIN, HIGH);
        digitalWrite(IN2_PIN, LOW);
        ledcWrite(PWM_CHANNEL_A, speed);
    } else if (speed < 0) {
        digitalWrite(IN1_PIN, LOW);
        digitalWrite(IN2_PIN, HIGH);
        ledcWrite(PWM_CHANNEL_A, -speed);
    } else {
        digitalWrite(IN1_PIN, LOW);
        digitalWrite(IN2_PIN, LOW);
        ledcWrite(PWM_CHANNEL_A, 0);
    }
}

void setMotorB(int speed) {
    speed = constrain(speed, -255, 255);
    if (speed > 0) {
        digitalWrite(IN3_PIN, HIGH);
        digitalWrite(IN4_PIN, LOW);
        ledcWrite(PWM_CHANNEL_B, speed);
    } else if (speed < 0) {
        digitalWrite(IN3_PIN, LOW);
        digitalWrite(IN4_PIN, HIGH);
        ledcWrite(PWM_CHANNEL_B, -speed);
    } else {
        digitalWrite(IN3_PIN, LOW);
        digitalWrite(IN4_PIN, LOW);
        ledcWrite(PWM_CHANNEL_B, 0);
    }
}

void stopMotors() {
    setMotorA(0);
    setMotorB(0);
    currentSpeedA = 0;
    currentSpeedB = 0;
    targetSpeedA = 0;
    targetSpeedB = 0;
}

int rampTowards(int current, int target, int step) {
    if (current < target) {
        return min(current + step, target);
    } else if (current > target) {
        return max(current - step, target);
    }
    return current;
}

void mixDrive(uint16_t throttleRaw, uint16_t steerRaw) {
    int throttle = map(throttleRaw, 0, JOY_MAX, -255, 255);
    int steer    = map(steerRaw,    0, JOY_MAX, -255, 255);

    // Deadzone
    if (abs(throttle) < JOY_DEADZONE / 16) throttle = 0;
    if (abs(steer)    < JOY_DEADZONE / 16) steer    = 0;

    // Arcade mix
    int left  = constrain(throttle + steer, -255, 255);
    int right = constrain(throttle - steer, -255, 255);

    targetSpeedA = left;
    targetSpeedB = right;
}

void updateMotors() {
    // Ramp towards target for smooth acceleration
    currentSpeedA = rampTowards(currentSpeedA, targetSpeedA, RAMP_STEP);
    currentSpeedB = rampTowards(currentSpeedB, targetSpeedB, RAMP_STEP);

    setMotorA(currentSpeedA);
    setMotorB(currentSpeedB);
}

// ============================================================================
//  LED Helper Functions
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

// ============================================================================
//  LED Animation Modes (all non-blocking)
// ============================================================================

// ---------- Mode 0: Solid Color Cycle ----------
void ledSolidColorCycle() {
    if (millis() - animTimer < 30) return;
    animTimer = millis();
    animHue++;
    fillBoth(CHSV(animHue, 255, 255));
}

// ---------- Mode 1: Rainbow Wave ----------
void ledRainbowWave() {
    if (millis() - animTimer < 20) return;
    animTimer = millis();
    animHue++;
    for (int i = 0; i < NUM_LEDS; i++) {
        CRGB color = CHSV(animHue + (i * 25), 255, 255); // Spread across 10 LEDs
        ledsLeft[i] = color;
        ledsRight[i] = color;
    }
}

// ---------- Mode 2: Breathing Pulse ----------
void ledBreathingPulse() {
    if (millis() - animTimer < 15) return;
    animTimer = millis();
    animStep++;
    // Smooth sine-wave breathing
    uint8_t val = (uint8_t)((sin(animStep * 0.05) + 1.0) * 127.5);
    fillBoth(CHSV(160, 255, val));  // Blue breathing
}

// ---------- Mode 3: Knight Rider (Larson Scanner) ----------
void ledKnightRider() {
    if (millis() - animTimer < 80) return;
    animTimer = millis();

    fadeBoth(100);

    int pos = animStep % NUM_LEDS;
    ledsLeft[pos] = CRGB::Red;
    ledsRight[pos] = CRGB::Red;

    animStep += animDirection;
    if (animStep >= NUM_LEDS - 1 || animStep <= 0) {
        animDirection = -animDirection;
    }
}

// ---------- Mode 4: Strobe Flash ----------
void ledStrobeFlash() {
    if (millis() - animTimer < 50) return;
    animTimer = millis();
    animStep++;
    if (animStep % 2 == 0) {
        fillBoth(CRGB::White);
    } else {
        fillBoth(CRGB::Black);
    }
}

// ---------- Mode 5: Fire Flicker ----------
void ledFireFlicker() {
    if (millis() - animTimer < 40) return;
    animTimer = millis();

    for (int i = 0; i < NUM_LEDS; i++) {
        // Random warm colors (red-orange-yellow spectrum)
        ledsLeft[i] = CHSV(random8(0, 40), 255, random8(100, 255));
        ledsRight[i] = CHSV(random8(0, 40), 255, random8(100, 255));
    }
}

// ---------- Mode 6: Color Wipe ----------
void ledColorWipe() {
    if (millis() - animTimer < 100) return;
    animTimer = millis();

    int pos = animStep % (NUM_LEDS * 2);  // Fill then clear

    if (pos < NUM_LEDS) {
        // Fill phase
        ledsLeft[pos] = CHSV(animHue, 255, 255);
        ledsRight[pos] = CHSV(animHue, 255, 255);
    } else {
        // Clear phase
        ledsLeft[pos - NUM_LEDS] = CRGB::Black;
        ledsRight[pos - NUM_LEDS] = CRGB::Black;
    }

    animStep++;
    if (animStep % (NUM_LEDS * 2) == 0) {
        animHue += 60;  // New color each cycle
    }
}

// ---------- Mode 7: Sparkle ----------
void ledSparkle() {
    if (millis() - animTimer < 60) return;
    animTimer = millis();

    fadeBoth(80);

    // Random sparkle on either side
    if (random8() < 120) {
        ledsLeft[random8(NUM_LEDS)] = CRGB::White;
    }
    if (random8() < 120) {
        ledsRight[random8(NUM_LEDS)] = CRGB::White;
    }
}

// ---------- Mode 8: Police Siren ----------
void ledPoliceSiren() {
    if (millis() - animTimer < 120) return;
    animTimer = millis();
    animStep++;

    int phase = (animStep / 3) % 4;  // 4 phases for alternating pattern

    switch (phase) {
        case 0: // Left flashes Red
            fill_solid(ledsLeft, NUM_LEDS, CRGB::Red);
            fill_solid(ledsRight, NUM_LEDS, CRGB::Black);
            break;
        case 1: // Both Black
            fillBoth(CRGB::Black);
            break;
        case 2: // Right flashes Blue
            fill_solid(ledsLeft, NUM_LEDS, CRGB::Black);
            fill_solid(ledsRight, NUM_LEDS, CRGB::Blue);
            break;
        case 3: // Both Black
            fillBoth(CRGB::Black);
            break;
    }
}

// ---------- Mode 9: Meteor Rain ----------
void ledMeteorRain() {
    if (millis() - animTimer < 60) return;
    animTimer = millis();

    for (int i = 0; i < NUM_LEDS; i++) {
        if (random8() < 128) {
            ledsLeft[i].fadeToBlackBy(120);
            ledsRight[i].fadeToBlackBy(120);
        }
    }

    int pos = animStep % (NUM_LEDS + 5);  // Extra steps for trail to fade
    if (pos < NUM_LEDS) {
        ledsLeft[pos] = CRGB::White;
        ledsRight[pos] = CRGB::White;
    }

    animStep++;
}

// ---------- Mode 10: Theater Chase Rainbow ----------
void ledTheaterChase() {
    if (millis() - animTimer < 80) return;
    animTimer = millis();
    animStep++;
    animHue++;

    fillBoth(CRGB::Black);
    for (int i = 0; i < NUM_LEDS; i++) {
        if ((i + animStep) % 3 == 0) {
            CRGB color = CHSV(animHue + (i * 25), 255, 255);
            ledsLeft[i] = color;
            ledsRight[i] = color;
        }
    }
}

// ---------- Mode 11: Static White ----------
void ledStaticWhite() {
    fillBoth(CRGB::White);
}

// ============================================================================
//  Motor-LED Sync Mode
// ============================================================================

void ledMotorSync(int throttle, int steer) {
    // Map raw joystick to signed values
    int fwd  = map(throttle, 0, JOY_MAX, -255, 255);
    int turn = map(steer,    0, JOY_MAX, -255, 255);

    // Deadzone
    if (abs(fwd)  < JOY_DEADZONE / 16) fwd  = 0;
    if (abs(turn) < JOY_DEADZONE / 16) turn = 0;

    int absFwd  = abs(fwd);
    int absTurn = abs(turn);
    int speed   = max(absFwd, absTurn);

    // Detect braking (rapid deceleration)
    bool braking = (prevThrottle > 100 && absFwd < 30);

    if (braking) {
        // ---- Braking: Red flash burst ----
        if (millis() - animTimer < 50) return;
        animTimer = millis();
        animStep++;
        if (animStep % 2 == 0) {
            fillBoth(CRGB::Red);
        } else {
            fillBoth(CRGB::Black);
        }
    }
    else if (speed > 230) {
        // ---- Full speed: Rapid rainbow pulse ----
        if (millis() - animTimer < 15) return;
        animTimer = millis();
        animHue += 5;
        for (int i = 0; i < NUM_LEDS; i++) {
            CRGB color = CHSV(animHue + (i * 25), 255, 255);
            ledsLeft[i] = color;
            ledsRight[i] = color;
        }
    }
    else if (fwd > 0) {
        // ---- Forward: Green sweep back→front ----
        if (millis() - animTimer < (unsigned long)map(absFwd, 30, 255, 100, 20)) return;
        animTimer = millis();
        animStep++;

        fadeBoth(100);
        int pos = animStep % NUM_LEDS;
        ledsLeft[pos] = CRGB::Green;
        ledsRight[pos] = CRGB::Green;

        // Turn tint overlays on top of the green sweep
        if (absTurn > 30) {
            if (turn > 0) {
                // Turning right — right side flashes orange
                fill_solid(ledsRight, NUM_LEDS, CRGB::Orange);
            } else {
                // Turning left — left side flashes orange
                fill_solid(ledsLeft, NUM_LEDS, CRGB::Orange);
            }
        }
    }
    else if (fwd < 0) {
        // ---- Reverse: Red sweep front→back ----
        if (millis() - animTimer < (unsigned long)map(absFwd, 30, 255, 100, 20)) return;
        animTimer = millis();
        animStep++;

        fadeBoth(100);
        int pos = (NUM_LEDS - 1) - (animStep % NUM_LEDS);
        ledsLeft[pos] = CRGB::Red;
        ledsRight[pos] = CRGB::Red;

        // Turn tint overlays on top of the red sweep
        if (absTurn > 30) {
            if (turn > 0) {
                fill_solid(ledsRight, NUM_LEDS, CRGB::Orange);
            } else {
                fill_solid(ledsLeft, NUM_LEDS, CRGB::Orange);
            }
        }
    }
    else if (absTurn > 30) {
        // ---- Turning only (no throttle): Orange blink ----
        if (millis() - animTimer < 150) return;
        animTimer = millis();
        animStep++;

        fillBoth(CRGB::Black);
        if (animStep % 2 == 0) {
            uint8_t bright = map(absTurn, 30, 255, 100, 255);
            if (turn > 0) {
                // Right turn — blink right strip orange
                fill_solid(ledsRight, NUM_LEDS, CHSV(25, 255, bright));
            } else {
                // Left turn — blink left strip orange
                fill_solid(ledsLeft, NUM_LEDS, CHSV(25, 255, bright));
            }
        }
    }
    else {
        // ---- Idle: Gentle green breathing ----
        if (millis() - animTimer < 20) return;
        animTimer = millis();
        animStep++;
        uint8_t val = (uint8_t)((sin(animStep * 0.04) + 1.0) * 100.0 + 20);
        fillBoth(CHSV(96, 255, val));  // Green hue
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
            case 0:  ledSolidColorCycle(); break;
            case 1:  ledRainbowWave();     break;
            case 2:  ledBreathingPulse();  break;
            case 3:  ledKnightRider();     break;
            case 4:  ledStrobeFlash();     break;
            case 5:  ledFireFlicker();     break;
            case 6:  ledColorWipe();       break;
            case 7:  ledSparkle();         break;
            case 8:  ledPoliceSiren();     break;
            case 9:  ledMeteorRain();      break;
            case 10: ledTheaterChase();    break;
            case 11: ledStaticWhite();     break;
            default: ledSolidColorCycle(); break;
        }
    }

    FastLED.show();
}

// ============================================================================
//  Button Edge Detection & Mode Cycling
// ============================================================================

void handleButtons() {
    // Sync led mode with transmitter's setting
    if (ledMode != incomingData.addrLedMode) {
        if (incomingData.addrLedMode < TOTAL_LED_MODES) {
            ledMode = incomingData.addrLedMode;
            animStep = 0;
            animTimer = 0;
            Serial.print("LED Mode (from remote setting) → ");
            Serial.println(ledMode);
        }
    }

    // Toggle1: motor-LED sync
    motorSyncEnabled = incomingData.toggle1;

    // Toggle2: motor enable/disable (handled in loop)
}

// ============================================================================
//  ESP-NOW Callbacks
// ============================================================================

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onDataRecv(const esp_now_recv_info_t *recvInfo, const uint8_t *data, int len) {
    if (recvInfo == nullptr || recvInfo->src_addr == nullptr) return;
    const uint8_t *mac = recvInfo->src_addr;
#else
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
#endif
    if (len != sizeof(struct_message)) return;

    struct_message tempData;
    memcpy(&tempData, data, sizeof(struct_message));

    // Verify key check (0 = accept all)
    if (EXPECTED_VERIFY_KEY != 0 && tempData.verifyKey != EXPECTED_VERIFY_KEY) {
        return;  // Reject packet from unpaired transmitter
    }

    memcpy(&incomingData, &tempData, sizeof(struct_message));
    lastRecvTime = millis();

    // Remember sender for feedback
    if (!hasSender) {
        memcpy(senderMac, mac, 6);
        hasSender = true;

        // Add sender as peer for feedback
        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, senderMac, 6);
        peerInfo.channel = 0;
        peerInfo.encrypt = false;
        if (!esp_now_is_peer_exist(senderMac)) {
            esp_now_add_peer(&peerInfo);
        }

        Serial.print("Paired with: ");
        char macStr[18];
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        Serial.println(macStr);
    }
}

// ============================================================================
//  Feedback to Transmitter
// ============================================================================

void sendFeedback() {
    if (!hasSender) return;
    if (millis() - lastFeedbackTime < FEEDBACK_INTERVAL) return;
    lastFeedbackTime = millis();

    feedbackData.data1 = (uint8_t)ledMode;
    feedbackData.data2 = motorSyncEnabled ? 1 : 0;
    feedbackData.data3 = (uint8_t)(map(analogRead(BATT_ADC_PIN), 0, 4095, 0, 255));

    esp_now_send(senderMac, (uint8_t *)&feedbackData, sizeof(rx_message));
}

// ============================================================================
//  Setup
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("====================================");
    Serial.println("  ESP-NOW Receiver v2.0-led");
    Serial.println("  L298N + Dual WS2812B (10 LEDs ea)");
    Serial.println("====================================");

    // Motor init
    initMotors();
    stopMotors();

    // Status LED
    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(STATUS_LED, LOW);

    // FastLED init for two separate strips
    FastLED.addLeds<LED_TYPE, LED_PIN_LEFT, COLOR_ORDER>(ledsLeft, NUM_LEDS)
           .setCorrection(TypicalLEDStrip);
    FastLED.addLeds<LED_TYPE, LED_PIN_RIGHT, COLOR_ORDER>(ledsRight, NUM_LEDS)
           .setCorrection(TypicalLEDStrip);
           
    FastLED.setBrightness(DEFAULT_BRIGHTNESS);
    fillBoth(CRGB::Black);
    FastLED.show();

    // WiFi + ESP-NOW
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    Serial.print("Receiver MAC: ");
    Serial.println(WiFi.macAddress());

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init FAILED!");
        return;
    }
    esp_now_register_recv_cb(onDataRecv);

    // Initialize incoming data to neutral
    incomingData.joyX     = JOY_CENTER;
    incomingData.joyY     = JOY_CENTER;
    incomingData.potValue = 0;
    incomingData.toggle1  = false;
    incomingData.toggle2  = false;
    incomingData.push1    = false;
    incomingData.push2    = false;
    incomingData.verifyKey = 0;
    incomingData.addrLedMode = 0;

    // Startup LED animation (quick rainbow sweep)
    for (int h = 0; h < 256; h += 8) {
        fillBoth(CHSV(h, 255, 200));
        FastLED.show();
        delay(10);
    }
    fillBoth(CRGB::Black);
    FastLED.show();

    Serial.println("Ready. Waiting for transmitter...");
}

// ============================================================================
//  Main Loop
// ============================================================================

void loop() {
    unsigned long now = millis();

    // ---- Safety timeout: stop everything if signal lost ----
    if (now - lastRecvTime > SIGNAL_TIMEOUT) {
        stopMotors();
        digitalWrite(STATUS_LED, LOW);

        // Signal-lost LED indicator: slow red pulse
        static unsigned long lostTimer = 0;
        static unsigned long lostStep  = 0;
        if (now - lostTimer > 30) {
            lostTimer = now;
            lostStep++;
            uint8_t val = (uint8_t)((sin(lostStep * 0.06) + 1.0) * 80.0);
            fillBoth(CRGB(val, 0, 0));
            FastLED.show();
        }
        delay(10);
        return;
    }

    // ---- Status LED: signal active ----
    digitalWrite(STATUS_LED, HIGH);

    // ---- Update brightness from potentiometer ----
    ledBrightness = map(incomingData.potValue, 0, 4095, 0, 255);
    FastLED.setBrightness(ledBrightness);

    // ---- Button handling ----
    handleButtons();

    // ---- Motor control ----
    if (incomingData.toggle2) {
        // Toggle2 ON = motors ENABLED (switches are inverted in transmitter: true = active)
        mixDrive(incomingData.joyY, incomingData.joyX);
        updateMotors();
    } else {
        // Toggle2 OFF = motors DISABLED (safety kill)
        stopMotors();
    }

    // ---- LED animations ----
    updateLEDs();

    // ---- Feedback to transmitter ----
    sendFeedback();

    delay(10);  // ~100Hz loop
}
