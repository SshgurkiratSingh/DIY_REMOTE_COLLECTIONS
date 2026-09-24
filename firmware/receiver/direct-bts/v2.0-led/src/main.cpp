/**
 * ============================================================================
 *  ESP-NOW Receiver v2.0-led (EYE MATRIX EDITION)
 *  L298N Motor Driver + Dual WS2812B LED Strips (10 LEDs each)
 * ============================================================================
 *
 *  Compatible with: Transmitter v2.0 (ESP-NOW OLED Remote)
 *  Board:           ESP32 DevKit
 *  Motor Driver:    L298N (dual H-bridge)
 *  LED Strip:       2x WS2812B (10 LEDs per eye)
 *
 *  Eye Layout:
 *    - Line 1: LEDs 0, 1, 2
 *    - Hidden: LED 3
 *    - Line 2: LEDs 4, 5, 6
 *    - Hidden: LED 7
 *    - Line 3: LEDs 8, 9
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
#define ENA_PIN   25
#define IN1_PIN   26
#define IN2_PIN   27
#define ENB_PIN   14
#define IN3_PIN   12
#define IN4_PIN   13

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

#define PWM_FREQ       5000
#define PWM_RESOLUTION 8
#define PWM_CHANNEL_A  0
#define PWM_CHANNEL_B  1

#define JOY_MAX        4095
#define JOY_CENTER     2048
#define JOY_DEADZONE   120
#define RAMP_STEP      15
#define SIGNAL_TIMEOUT 1000
#define FEEDBACK_INTERVAL 500

#define DEFAULT_BRIGHTNESS 128
#define TOTAL_LED_MODES    10

#define EXPECTED_VERIFY_KEY 0

// ============================================================================
//  Data Structures
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
    uint8_t  addrLedMode; // Selected via transmitter settings
} struct_message;

typedef struct rx_message {
    uint8_t data1;   // Current LED mode
    uint8_t data2;   // Motor-LED sync state
    uint8_t data3;   // Battery
} rx_message;

// ============================================================================
//  Global State
// ============================================================================

struct_message incomingData;
rx_message     feedbackData;
unsigned long  lastRecvTime = 0;
uint8_t        senderMac[6];
bool           hasSender = false;

int   targetSpeedA   = 0;
int   targetSpeedB   = 0;
int   currentSpeedA  = 0;
int   currentSpeedB  = 0;
int   prevThrottle   = 0;

CRGB  ledsLeft[NUM_LEDS];
CRGB  ledsRight[NUM_LEDS];
int   ledMode        = 0;
uint8_t ledBrightness = DEFAULT_BRIGHTNESS;
bool  motorSyncEnabled = false;

unsigned long animTimer      = 0;
unsigned long animStep       = 0;
int           animDirection  = 1;
uint8_t       animHue        = 0;

unsigned long lastFeedbackTime = 0;

// ============================================================================
//  Motor Control (L298N)
// ============================================================================

void initMotors() {
    pinMode(IN1_PIN, OUTPUT);
    pinMode(IN2_PIN, OUTPUT);
    pinMode(IN3_PIN, OUTPUT);
    pinMode(IN4_PIN, OUTPUT);
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
    if (current < target) return min(current + step, target);
    else if (current > target) return max(current - step, target);
    return current;
}

void mixDrive(uint16_t throttleRaw, uint16_t steerRaw) {
    int throttle = map(throttleRaw, 0, JOY_MAX, -255, 255);
    int steer    = map(steerRaw,    0, JOY_MAX, -255, 255);

    if (abs(throttle) < JOY_DEADZONE / 16) throttle = 0;
    if (abs(steer)    < JOY_DEADZONE / 16) steer    = 0;

    int left  = constrain(throttle + steer, -255, 255);
    int right = constrain(throttle - steer, -255, 255);

    targetSpeedA = left;
    targetSpeedB = right;
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

// Applies the blackout mask to the 4th (idx 3) and 8th (idx 7) hidden LEDs
void maskHiddenLEDs() {
    ledsLeft[3] = CRGB::Black;
    ledsLeft[7] = CRGB::Black;
    ledsRight[3] = CRGB::Black;
    ledsRight[7] = CRGB::Black;
}

// ============================================================================
//  10 Custom Eye Animations
// ============================================================================

// Mode 0: Solid Headlights (White on all visible)
void eyeSolidHeadlights() {
    fillBoth(CRGB::White);
}

// Mode 1: Angry Eyes (Red on top and middle lines, bottom off)
void eyeAngry() {
    fillBoth(CRGB::Black);
    for (int i = 0; i < 3; i++) { // Line 1
        ledsLeft[i] = CRGB::Red;
        ledsRight[i] = CRGB::Red;
    }
    for (int i = 4; i < 7; i++) { // Line 2
        ledsLeft[i] = CRGB::Red;
        ledsRight[i] = CRGB::Red;
    }
}

// Mode 2: Scanning Pupil (Sweeping left and right across columns)
void eyeScanning() {
    if (millis() - animTimer < 120) return;
    animTimer = millis();

    fadeBoth(150);
    
    // Column indices (0=left, 1=center, 2=right)
    int col = animStep % 3;
    
    // Map column to LEDs in the 3 lines
    // Line 1: 0,1,2 | Line 2: 4,5,6 | Line 3: 8,9 (center-aligned mostly)
    int led1 = col;           // Line 1
    int led2 = 4 + col;       // Line 2
    int led3 = (col == 1) ? 8 : (col == 2 ? 9 : -1); // Line 3 has only 2 LEDs

    ledsLeft[led1] = CRGB::Red;
    ledsRight[led1] = CRGB::Red;
    ledsLeft[led2] = CRGB::Red;
    ledsRight[led2] = CRGB::Red;
    
    if (led3 != -1) {
        ledsLeft[led3] = CRGB::Red;
        ledsRight[led3] = CRGB::Red;
    }

    animStep += animDirection;
    if (animStep >= 2 || animStep <= 0) animDirection = -animDirection;
}

// Mode 3: Natural Blinking (Solid white, blinks off briefly)
void eyeBlinking() {
    // 3 seconds open, 150ms blink
    int cycle = (millis() / 150) % 20; 
    if (cycle == 0) {
        fillBoth(CRGB::Black); // Eyes closed
    } else {
        fillBoth(CRGB::White); // Eyes open
    }
}

// Mode 4: Sleepy Breathing (Bottom two lines only, slow cyan pulse)
void eyeSleepyBreathing() {
    if (millis() - animTimer < 20) return;
    animTimer = millis();
    animStep++;
    
    fillBoth(CRGB::Black);
    uint8_t val = (uint8_t)((sin(animStep * 0.03) + 1.0) * 100.0 + 30);
    
    // Light up Line 2 and Line 3 only
    for (int i = 4; i < 7; i++) { ledsLeft[i] = CHSV(160, 255, val); ledsRight[i] = CHSV(160, 255, val); }
    for (int i = 8; i < 10; i++) { ledsLeft[i] = CHSV(160, 255, val); ledsRight[i] = CHSV(160, 255, val); }
}

// Mode 5: Rainbow Flow
void eyeRainbow() {
    if (millis() - animTimer < 30) return;
    animTimer = millis();
    animHue++;
    for (int i = 0; i < NUM_LEDS; i++) {
        CRGB color = CHSV(animHue + (i * 20), 255, 255);
        ledsLeft[i] = color;
        ledsRight[i] = color;
    }
}

// Mode 6: Hypnotic Lines (Cycles Line 1 -> 2 -> 3)
void eyeHypnotic() {
    if (millis() - animTimer < 100) return;
    animTimer = millis();
    animStep++;
    
    fadeBoth(80);
    int phase = animStep % 3;
    CRGB color = CHSV(animStep * 10, 255, 255);

    if (phase == 0) { // Line 1
        for(int i=0; i<3; i++) { ledsLeft[i] = color; ledsRight[i] = color; }
    } else if (phase == 1) { // Line 2
        for(int i=4; i<7; i++) { ledsLeft[i] = color; ledsRight[i] = color; }
    } else { // Line 3
        for(int i=8; i<10; i++) { ledsLeft[i] = color; ledsRight[i] = color; }
    }
}

// Mode 7: Fire Flicker
void eyeFire() {
    if (millis() - animTimer < 50) return;
    animTimer = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
        ledsLeft[i] = CHSV(random8(0, 40), 255, random8(100, 255));
        ledsRight[i] = CHSV(random8(0, 40), 255, random8(100, 255));
    }
}

// Mode 8: Police Strobe (Left Red, Right Blue)
void eyePoliceStrobe() {
    if (millis() - animTimer < 100) return;
    animTimer = millis();
    animStep++;

    int phase = (animStep / 2) % 4; 
    switch (phase) {
        case 0: fill_solid(ledsLeft, NUM_LEDS, CRGB::Red); fill_solid(ledsRight, NUM_LEDS, CRGB::Black); break;
        case 1: fillBoth(CRGB::Black); break;
        case 2: fill_solid(ledsLeft, NUM_LEDS, CRGB::Black); fill_solid(ledsRight, NUM_LEDS, CRGB::Blue); break;
        case 3: fillBoth(CRGB::Black); break;
    }
}

// Mode 9: Cyber Sparkle
void eyeSparkle() {
    if (millis() - animTimer < 40) return;
    animTimer = millis();
    fadeBoth(60);
    if (random8() < 120) ledsLeft[random8(NUM_LEDS)] = CHSV(random8(), 200, 255);
    if (random8() < 120) ledsRight[random8(NUM_LEDS)] = CHSV(random8(), 200, 255);
}

// ============================================================================
//  Motor-LED Sync Mode
// ============================================================================

void ledMotorSync(int throttle, int steer) {
    int fwd  = map(throttle, 0, JOY_MAX, -255, 255);
    int turn = map(steer,    0, JOY_MAX, -255, 255);

    if (abs(fwd)  < JOY_DEADZONE / 16) fwd  = 0;
    if (abs(turn) < JOY_DEADZONE / 16) turn = 0;

    int absFwd  = abs(fwd);
    int absTurn = abs(turn);
    int speed   = max(absFwd, absTurn);
    bool braking = (prevThrottle > 100 && absFwd < 30);

    if (braking) {
        if (millis() - animTimer < 50) return;
        animTimer = millis();
        animStep++;
        if (animStep % 2 == 0) fillBoth(CRGB::Red);
        else fillBoth(CRGB::Black);
    }
    else if (speed > 230) {
        if (millis() - animTimer < 15) return;
        animTimer = millis();
        animHue += 5;
        for (int i = 0; i < NUM_LEDS; i++) {
            CRGB color = CHSV(animHue + (i * 25), 255, 255);
            ledsLeft[i] = color;
            ledsRight[i] = color;
        }
    }
    else if (fwd > 0 || fwd < 0) {
        if (millis() - animTimer < (unsigned long)map(absFwd, 30, 255, 100, 20)) return;
        animTimer = millis();
        animStep++;
        fadeBoth(100);
        
        // Randomly sparkle green for forward, red for reverse across the matrix
        CRGB driveColor = (fwd > 0) ? CRGB::Green : CRGB::Red;
        ledsLeft[random8(NUM_LEDS)] = driveColor;
        ledsRight[random8(NUM_LEDS)] = driveColor;

        if (absTurn > 30) {
            if (turn > 0) fill_solid(ledsRight, NUM_LEDS, CRGB::Orange);
            else fill_solid(ledsLeft, NUM_LEDS, CRGB::Orange);
        }
    }
    else if (absTurn > 30) {
        if (millis() - animTimer < 150) return;
        animTimer = millis();
        animStep++;
        fillBoth(CRGB::Black);
        if (animStep % 2 == 0) {
            uint8_t bright = map(absTurn, 30, 255, 100, 255);
            if (turn > 0) fill_solid(ledsRight, NUM_LEDS, CHSV(25, 255, bright));
            else fill_solid(ledsLeft, NUM_LEDS, CHSV(25, 255, bright));
        }
    }
    else {
        if (millis() - animTimer < 20) return;
        animTimer = millis();
        animStep++;
        uint8_t val = (uint8_t)((sin(animStep * 0.04) + 1.0) * 100.0 + 20);
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
            case 1: eyeAngry(); break;
            case 2: eyeScanning(); break;
            case 3: eyeBlinking(); break;
            case 4: eyeSleepyBreathing(); break;
            case 5: eyeRainbow(); break;
            case 6: eyeHypnotic(); break;
            case 7: eyeFire(); break;
            case 8: eyePoliceStrobe(); break;
            case 9: eyeSparkle(); break;
            default: eyeSolidHeadlights(); break;
        }
    }
    
    // Apply blackout mask to hidden LEDs before sending data
    maskHiddenLEDs();
    FastLED.show();
}

// ============================================================================
//  Sync Data
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

    if (EXPECTED_VERIFY_KEY != 0 && tempData.verifyKey != EXPECTED_VERIFY_KEY) {
        return;
    }

    memcpy(&incomingData, &tempData, sizeof(struct_message));
    lastRecvTime = millis();

    if (!hasSender) {
        memcpy(senderMac, mac, 6);
        hasSender = true;

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
//  Setup & Loop
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("====================================");
    Serial.println("  ESP-NOW Receiver v2.0-led");
    Serial.println("  L298N + Dual WS2812B Eye Matrix");
    Serial.println("====================================");

    initMotors();
    stopMotors();

    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(STATUS_LED, LOW);

    FastLED.addLeds<LED_TYPE, LED_PIN_LEFT, COLOR_ORDER>(ledsLeft, NUM_LEDS)
           .setCorrection(TypicalLEDStrip);
    FastLED.addLeds<LED_TYPE, LED_PIN_RIGHT, COLOR_ORDER>(ledsRight, NUM_LEDS)
           .setCorrection(TypicalLEDStrip);
           
    FastLED.setBrightness(DEFAULT_BRIGHTNESS);
    fillBoth(CRGB::Black);
    FastLED.show();

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    Serial.print("Receiver MAC: ");
    Serial.println(WiFi.macAddress());

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init FAILED!");
        return;
    }
    esp_now_register_recv_cb(onDataRecv);

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
        maskHiddenLEDs();
        FastLED.show();
        delay(10);
    }
    fillBoth(CRGB::Black);
    maskHiddenLEDs();
    FastLED.show();

    Serial.println("Ready. Waiting for transmitter...");
}

void loop() {
    unsigned long now = millis();

    if (now - lastRecvTime > SIGNAL_TIMEOUT) {
        stopMotors();
        digitalWrite(STATUS_LED, LOW);

        static unsigned long lostTimer = 0;
        static unsigned long lostStep  = 0;
        if (now - lostTimer > 30) {
            lostTimer = now;
            lostStep++;
            uint8_t val = (uint8_t)((sin(lostStep * 0.06) + 1.0) * 80.0);
            fillBoth(CRGB(val, 0, 0));
            maskHiddenLEDs();
            FastLED.show();
        }
        delay(10);
        return;
    }

    digitalWrite(STATUS_LED, HIGH);

    ledBrightness = map(incomingData.potValue, 0, 4095, 0, 255);
    FastLED.setBrightness(ledBrightness);

    handleButtons();

    if (incomingData.toggle2) {
        mixDrive(incomingData.joyY, incomingData.joyX);
        updateMotors();
    } else {
        stopMotors();
    }

    updateLEDs();
    sendFeedback();

    delay(10);
}
