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
    }
    // In running mode, GPIO 2 is used as analog sensor S4 (Center). Do not drive it!
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
    Serial.println("   Captive Portal Calibration System v1.1");
    Serial.println("   Rulebook Competition & Maze Engine");
    Serial.println("==============================================");

    // Pin configuration
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

    // 9-sensor array — all 9 sensors initialized as INPUT
    static const uint8_t SENSOR_PINS_MAIN[NUM_SENSORS] = {
        SENSOR_S0_PIN, SENSOR_S1_PIN, SENSOR_S2_PIN,
        SENSOR_S3_PIN, SENSOR_S4_PIN, SENSOR_S5_PIN,
        SENSOR_S6_PIN, SENSOR_S7_PIN, SENSOR_S8_PIN
    };
    for (int i = 0; i < NUM_SENSORS; i++) {
        pinMode(SENSOR_PINS_MAIN[i], INPUT);
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
        // Put TB6612 into standby mode so status LED blinker on GPIO 2 doesn't energize Motor A
        setMotorStandby(true);
        initWebPortal();
        emergencyStop();
    } else {
        // Running mode: Enable TB6612 motor driver (WiFi is OFF, ADC2 is fully usable)
        setMotorStandby(false);
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

        // Feed sensor updates for calibration sweep (6 ADC1 sensors: 2 Left, 3 Center, 1 Right)
        if (isCalibrating()) {
            uint16_t rawAll[NUM_SENSORS] = {0};
            // Left Group (2 on ADC1): S1, S2 (S0 is ADC2, remains 0)
            rawAll[1] = analogRead(SENSOR_S1_PIN);
            rawAll[2] = analogRead(SENSOR_S2_PIN);
            // Center Group (3 on ADC1): S3, S4, S5 (all active!)
            rawAll[3] = analogRead(SENSOR_S3_PIN);
            rawAll[4] = analogRead(SENSOR_S4_PIN);
            rawAll[5] = analogRead(SENSOR_S5_PIN);
            // Right Group (1 on ADC1): S6 (S7 and S8 are ADC2, remain 0)
            rawAll[6] = analogRead(SENSOR_S6_PIN);
            updateCalibration(rawAll, false); // adc2Available=false
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
                // 3.. 2.. 1.. countdown (serial debug only; GPIO2 is S4 sensor input)
                for (int i = 3; i > 0; i--) {
                    Serial.printf("[START] %d...\n", i);
                    delay(1000);
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

            // Read all 9 analog IR sensors (ADC2 is safe — WiFi is OFF in running mode)
            uint16_t rawAll[NUM_SENSORS];
            rawAll[0] = analogRead(SENSOR_S0_PIN);
            rawAll[1] = analogRead(SENSOR_S1_PIN);
            rawAll[2] = analogRead(SENSOR_S2_PIN);
            rawAll[3] = analogRead(SENSOR_S3_PIN);
            rawAll[4] = analogRead(SENSOR_S4_PIN);
            rawAll[5] = analogRead(SENSOR_S5_PIN);
            rawAll[6] = analogRead(SENSOR_S6_PIN);
            rawAll[7] = analogRead(SENSOR_S7_PIN);
            rawAll[8] = analogRead(SENSOR_S8_PIN);

            // Feed motor safety watchdog
            notifyMotorSafetyFeed();

            if (s_hasValidCalibration) {
                int16_t leftSpeed  = 0;
                int16_t rightSpeed = 0;

                // Compute PID response with all 9 sensors active (adc2Available = true)
                g_pid.compute(rawAll, true, leftSpeed, rightSpeed);

                // Check if Zone B (End Point) has been reached
                if (g_pid.isFinished()) {
                    emergencyStop();
                    s_runState    = RUN_STATE_FINISHED;
                    s_runDuration = millis() - s_runStartTime;

                    SensorReadings sr = g_pid.getReadings();
                    Serial.println("\n==============================================");
                    Serial.println("   OFFICIAL RUN COMPLETED!");
                    Serial.printf( "   Time Elapsed: %.2f seconds\n", s_runDuration / 1000.0f);
                    Serial.printf( "   Checkpoints Reached: %u / 4\n", sr.checkpoints);
                    Serial.println("==============================================\n");
                    return;
                }

                // Drive L298N motors
                setMotorSpeed(leftSpeed, rightSpeed);

                // Periodic telemetry (every 500ms) — print all 9 sensor norms
                static uint32_t s_lastLog = 0;
                if (now - s_lastLog >= 500) {
                    s_lastLog = now;
                    SensorReadings sr = g_pid.getReadings();
                    Serial.printf("[LFR] Norm:[%3u,%3u,%3u,%3u,%3u,%3u,%3u,%3u,%3u] Pos:%5d L:%4d R:%4d %s CP:%u/4\n",
                                  sr.norm[0], sr.norm[1], sr.norm[2],
                                  sr.norm[3], sr.norm[4], sr.norm[5],
                                  sr.norm[6], sr.norm[7], sr.norm[8],
                                  sr.position, leftSpeed, rightSpeed,
                                  PIDController::statusToString(sr.status),
                                  sr.checkpoints);
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