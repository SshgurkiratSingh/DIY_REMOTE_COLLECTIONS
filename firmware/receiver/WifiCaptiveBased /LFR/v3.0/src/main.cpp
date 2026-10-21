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

enum RunState {
    RUN_STATE_READY,     // Ready at Zone A, waiting for physical BOOT button press
    RUN_STATE_ACTIVE,    // Run started, autonomously navigating track
    RUN_STATE_FINISHED   // Zone B (End point) reached, stopped
};

static OperatingMode s_mode = MODE_RUNNING;
static RunState s_runState = RUN_STATE_READY;
static bool s_hasValidCalibration = false;

static uint32_t s_runStartTime = 0;
static uint32_t s_runDuration = 0;

// LED Blinker helper
static uint32_t s_lastLedToggle = 0;
static bool s_ledState = false;

static void updateStatusLed() {
    uint32_t now = millis();
    if (s_mode == MODE_CALIBRATION) {
        // Fast blink: 100ms interval (Calibration active)
        if (now - s_lastLedToggle >= 100) {
            s_lastLedToggle = now;
            s_ledState = !s_ledState;
            digitalWrite(STATUS_LED_PIN, s_ledState ? HIGH : LOW);
        }
    } else {
        // Running Mode
        if (s_runState == RUN_STATE_READY) {
            // Heartbeat: 2 quick blinks every 1200ms (Ready for button press)
            uint32_t phase = now % 1200;
            bool on = (phase < 80) || (phase >= 180 && phase < 260);
            digitalWrite(STATUS_LED_PIN, on ? HIGH : LOW);
        } else if (s_runState == RUN_STATE_ACTIVE) {
            // Solid ON while running on line
            digitalWrite(STATUS_LED_PIN, HIGH);
        } else if (s_runState == RUN_STATE_FINISHED) {
            // Slow blink: 400ms toggle (Run finished / Goal reached)
            if (now - s_lastLedToggle >= 400) {
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
    Serial.printf("   Captive Portal Calibration System %s\n", LFR_VERSION);
    Serial.println("   5-IR ADC1 Sensor Array & Maze Engine");
    Serial.println("==============================================");

    // Pin configuration
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

    // Configure all 5 IR sensor pins (all on ADC1)
    for (int i = 0; i < NUM_SENSORS; i++) {
        pinMode(SENSOR_PINS[i], INPUT);
    }

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
        s_runState = RUN_STATE_READY;
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
        Serial.println("[LFR] Robot is in READY state at Zone A.");
        Serial.println("[LFR] Press physical BOOT button (GPIO 0) on judge's signal to start run.");
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
            uint16_t raw[NUM_SENSORS];
            for (int i = 0; i < NUM_SENSORS; i++) {
                raw[i] = readSensorRaw(SENSOR_PINS[i]);
            }
            updateCalibration(raw);
        }

        // Keep motor safety verified (stops tests after timeout)
        checkMotorSafety();

        delay(2);
    } else {
        // ====================================================================
        //  RUNNING MODE - Competition Procedure
        // ====================================================================
        uint32_t now = millis();

        // Check BOOT button (physical switch rule)
        if (digitalRead(BOOT_BUTTON_PIN) == LOW) {
            uint32_t btnStart = millis();
            while (digitalRead(BOOT_BUTTON_PIN) == LOW) {
                // If held > 2s: Reboot to Calibration Mode
                if (millis() - btnStart > 2000) {
                    Serial.println("[BOOT] Button held 2s! Rebooting to Calibration Mode...");
                    emergencyStop();
                    forceCalibrationNextBoot();
                    delay(100);
                    ESP.restart();
                }
                delay(10);
            }

            // Short button press detected:
            if (s_runState == RUN_STATE_READY) {
                Serial.println("[START] Physical button pressed! Starting 3-second countdown...");
                // 3.. 2.. 1.. countdown with LED
                for (int i = 3; i > 0; i--) {
                    Serial.printf("[START] %d...\n", i);
                    digitalWrite(STATUS_LED_PIN, HIGH);
                    delay(200);
                    digitalWrite(STATUS_LED_PIN, LOW);
                    delay(800);
                }

                s_runStartTime = millis();
                s_runState = RUN_STATE_ACTIVE;
                g_pid.reset();
                notifyMotorSafetyFeed();
                Serial.println("[START] GO! Run timer started. Max duration: 10 minutes.");
            } else if (s_runState == RUN_STATE_FINISHED) {
                // If robot discovered a simplified shortest path in autonomous run:
                const char* solved = g_pid.getSolvedPath();
                if (solved && strlen(solved) > 0) {
                    Serial.printf("[RUN 2 READY] Arming robot with discovered shortest route: %s\n", solved);
                    strncpy(g_params.customPath, solved, sizeof(g_params.customPath) - 1);
                    g_params.customPath[sizeof(g_params.customPath) - 1] = '\0';
                    g_params.navMode = NAV_MODE_CUSTOM_PATH;
                    saveParameters(g_params);
                    g_pid.updateParams(g_params);
                }

                s_runState = RUN_STATE_READY;
                g_pid.reset();
                emergencyStop();
                Serial.println("[LFR] Reset to READY state at Zone A.");
            }
        }

        // If in READY or FINISHED: Keep motors stopped
        if (s_runState != RUN_STATE_ACTIVE) {
            emergencyStop();
            delay(10);
            return;
        }

        // ====================================================================
        //  ACTIVE RUN: 100Hz (10ms) PID Line Following & Maze Navigation Loop
        // ====================================================================
        static uint32_t s_lastLoopTick = 0;

        if (now - s_lastLoopTick >= (1000 / LOOP_RATE_HZ)) {
            s_lastLoopTick = now;

            // Check 10-minute maximum run duration rule
            if (now - s_runStartTime > (10UL * 60UL * 1000UL)) {
                emergencyStop();
                s_runState = RUN_STATE_FINISHED;
                s_runDuration = now - s_runStartTime;
                Serial.println("[TIMEOUT] 10-minute maximum run duration reached. Motors halted.");
                return;
            }

            // Read all 5 IR sensors
            uint16_t raw[NUM_SENSORS];
            for (int i = 0; i < NUM_SENSORS; i++) {
                raw[i] = readSensorRaw(SENSOR_PINS[i]);
            }

            // Feed motor safety watchdog
            notifyMotorSafetyFeed();

            if (s_hasValidCalibration) {
                int16_t leftSpeed = 0;
                int16_t rightSpeed = 0;

                // Compute PID response and maze navigation with 5 sensors
                g_pid.compute(raw, leftSpeed, rightSpeed);

                // Check if Zone B (End Point) has been reached
                if (g_pid.isFinished()) {
                    emergencyStop();
                    s_runState = RUN_STATE_FINISHED;
                    s_runDuration = millis() - s_runStartTime;

                    SensorReadings sr = g_pid.getReadings();
                    Serial.println("\n==============================================");
                    Serial.println("   OFFICIAL RUN COMPLETED!");
                    Serial.printf("   Time Elapsed: %.2f seconds\n", s_runDuration / 1000.0f);
                    Serial.printf("   Checkpoints Reached: %u / 4\n", sr.checkpoints);
                    Serial.println("==============================================\n");
                    return;
                }

                // Drive L298N motors
                setMotorSpeed(leftSpeed, rightSpeed);

                // Periodic telemetry debug output (every 500ms)
                static uint32_t s_lastLog = 0;
                if (now - s_lastLog >= 500) {
                    s_lastLog = now;
                    SensorReadings sr = g_pid.getReadings();
                    Serial.printf("[LFR 5-IR] Pos:%5d | Speeds:[L:%4d, R:%4d] | Status:%s | CP:%u/4\n",
                                  sr.position,
                                  leftSpeed, rightSpeed,
                                  PIDController::statusToString(sr.status),
                                  sr.checkpoints);
                    Serial.print("      Norm[0-4]: ");
                    for (int i = 0; i < NUM_SENSORS; i++) {
                        Serial.printf("%3u ", sr.norm[i]);
                    }
                    Serial.println();
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

        delay(1);
    }
}
