#include <Arduino.h>
#include "config.h"
#include "calibration.h"
#include "motor.h"
#include "pid.h"
#include "webportal.h"

enum OperatingMode {
    MODE_CALIBRATION,
    MODE_RUNNING
};

static OperatingMode s_mode = MODE_RUNNING;
static bool s_hasValidCalibration = false;

// LED Blinker helper
static uint32_t s_lastLedToggle = 0;
static bool s_ledState = false;

static void updateStatusLed() {
    uint32_t now = millis();
    if (s_mode == MODE_CALIBRATION) {
        // Fast blink: 100ms interval
        if (now - s_lastLedToggle >= 100) {
            s_lastLedToggle = now;
            s_ledState = !s_ledState;
            digitalWrite(STATUS_LED_PIN, s_ledState ? HIGH : LOW);
        }
    } else {
        // Running Mode
        if (s_hasValidCalibration) {
            // Solid ON
            digitalWrite(STATUS_LED_PIN, HIGH);
        } else {
            // Slow blink: 600ms interval (no valid calibration data)
            if (now - s_lastLedToggle >= 600) {
                s_lastLedToggle = now;
                s_ledState = !s_ledState;
                digitalWrite(STATUS_LED_PIN, s_ledState ? HIGH : LOW);
            }
        }
    }
}

// Check if BOOT button is held down at startup or during run
static bool checkBootButtonHeld(uint32_t holdTimeMs) {
    if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
        uint32_t start = millis();
        while (digitalRead(BOOT_BUTTON_PIN) == LOW) {
            if (millis() - start >= holdTimeMs) {
                return true;
            }
            delay(10);
        }
    }
    return false;
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n==============================================");
    Serial.println("   ESP32 PID LINE FOLLOWING ROBOT (LFR)");
    Serial.println("   Captive Portal Calibration System v1.0");
    Serial.println("==============================================");

    // Pin configuration
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

    pinMode(SENSOR_LEFT_PIN, INPUT);
    pinMode(SENSOR_CENTER_PIN, INPUT);
    pinMode(SENSOR_RIGHT_PIN, INPUT);

    // Initialize Motor subsystem
    initMotors();
    emergencyStop();

    // Initialize Preferences & Load stored parameters
    initPreferences();
    loadParameters(g_params);
    s_hasValidCalibration = isCalibrationValid(g_params);

    // Update persistent boot counter
    uint32_t bootCount = incrementBootCount();

    // Determine operating mode
    bool forceCalib = false;

    // 1. Check if BOOT button is held on startup
    Serial.println("[BOOT] Checking BOOT button (hold 1s to force calibration)...");
    if (checkBootButtonHeld(FORCE_CALIB_HOLD_TIME_MS)) {
        forceCalib = true;
        Serial.println("[BOOT] Force calibration requested via BOOT button!");
    }

    // 2. Automatic trigger every 5th boot or if calibration was forced
    if (forceCalib || (bootCount % CALIBRATION_BOOT_INTERVAL == 0)) {
        s_mode = MODE_CALIBRATION;
        Serial.printf("[BOOT] Entering CALIBRATION MODE (Boot Count = %u, Interval = %u)\n",
                      bootCount, CALIBRATION_BOOT_INTERVAL);
    } else {
        s_mode = MODE_RUNNING;
        Serial.printf("[BOOT] Entering RUNNING MODE (Boot Count = %u). Valid calibration = %s\n",
                      bootCount, s_hasValidCalibration ? "YES" : "NO");
    }

    if (s_mode == MODE_CALIBRATION) {
        initWebPortal();
        emergencyStop();
    } else {
        // Initialize PID controller with loaded parameters
        g_pid.init(g_params);
        notifyMotorSafetyFeed();
        Serial.println("[LFR] Starting Line Following Control Loop at 100Hz...");
    }
}

void loop() {
    // Keep status LED updated according to active state
    updateStatusLed();

    if (s_mode == MODE_CALIBRATION) {
        // Handle captive portal DNS & Web requests
        handleWebPortalLoop();

        // Feed sensor updates for calibration sweep if active
        if (isCalibrating()) {
            uint16_t rawL = analogRead(SENSOR_LEFT_PIN);
            uint16_t rawC = analogRead(SENSOR_CENTER_PIN);
            uint16_t rawR = analogRead(SENSOR_RIGHT_PIN);
            updateCalibration(rawL, rawC, rawR);
        }

        // Keep motor safety verified (stops tests after timeout)
        checkMotorSafety();

        delay(2);
    } else {
        // ====================================================================
        //  RUNNING MODE - 100Hz (10ms) PID Line Following Loop
        // ====================================================================
        static uint32_t s_lastLoopTick = 0;
        uint32_t now = millis();

        if (now - s_lastLoopTick >= (1000 / LOOP_RATE_HZ)) {
            s_lastLoopTick = now;

            // Read analog IR sensors
            uint16_t rawL = analogRead(SENSOR_LEFT_PIN);
            uint16_t rawC = analogRead(SENSOR_CENTER_PIN);
            uint16_t rawR = analogRead(SENSOR_RIGHT_PIN);

            // Feed motor safety watchdog
            notifyMotorSafetyFeed();

            if (s_hasValidCalibration) {
                int16_t leftSpeed = 0;
                int16_t rightSpeed = 0;

                // Compute PID response and edge cases
                g_pid.compute(rawL, rawC, rawR, leftSpeed, rightSpeed);

                // Drive L298N motors
                setMotorSpeed(leftSpeed, rightSpeed);

                // Periodic telemetry debug output (every 500ms)
                static uint32_t s_lastLog = 0;
                if (now - s_lastLog >= 500) {
                    s_lastLog = now;
                    SensorReadings sr = g_pid.getReadings();
                    Serial.printf("[LFR] Raw:[%4u,%4u,%4u] Norm:[%3u,%3u,%3u] Pos:%5d Speeds:[L:%4d, R:%4d] Status:%s\n",
                                  sr.rawL, sr.rawC, sr.rawR,
                                  sr.normL, sr.normC, sr.normR,
                                  sr.position,
                                  leftSpeed, rightSpeed,
                                  PIDController::statusToString(sr.status));
                }
            } else {
                // No valid calibration data: stop motors and wait for calibration
                emergencyStop();
                static uint32_t s_lastWarn = 0;
                if (now - s_lastWarn >= 2000) {
                    s_lastWarn = now;
                    Serial.println("[WARNING] No valid calibration found! Hold BOOT button or wait for 5th boot to calibrate.");
                }
            }
        }

        // Safety watchdog: halts motors if sensor loop stalls > 500ms
        checkMotorSafety();

        // Check if user holds BOOT button during run to switch to calibration mode
        if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
            static uint32_t s_btnPressStart = 0;
            if (s_btnPressStart == 0) s_btnPressStart = millis();
            if (millis() - s_btnPressStart > 2000) {
                Serial.println("[BOOT] BOOT button held 2s during run! Rebooting into Calibration Mode...");
                emergencyStop();
                forceCalibrationNextBoot();
                delay(100);
                ESP.restart();
            }
        } else {
            // Button released
            static uint32_t s_btnPressStart = 0;
            s_btnPressStart = 0;
        }

        delay(1);
    }
}