#include "calibration.h"
#include <Preferences.h>
#include "pid.h"

RobotParameters g_params;

static Preferences s_prefs;
static bool s_calibrating = false;
static uint32_t s_calibStartTime = 0;
static uint32_t s_calibDuration = CALIBRATION_DURATION_MS;

static uint16_t s_sweepMin[NUM_SENSORS];
static uint16_t s_sweepMax[NUM_SENSORS];

void initPreferences() {
    s_prefs.begin(PREFERENCES_NAMESPACE, false);
}

uint32_t getBootCount() {
    return s_prefs.getUInt("boot_count", 0);
}

uint32_t incrementBootCount() {
    uint32_t count = getBootCount();
    count++;
    s_prefs.putUInt("boot_count", count);
    Serial.printf("[NVS] Boot counter updated: %u\n", count);
    return count;
}

void resetBootCount() {
    s_prefs.putUInt("boot_count", 0);
    Serial.println("[NVS] Boot counter reset to 0");
}

void forceCalibrationNextBoot() {
    // Set to 4 so next boot increments to 5 (or interval) and triggers calibration
    s_prefs.putUInt("boot_count", CALIBRATION_BOOT_INTERVAL - 1);
    Serial.println("[NVS] Calibration scheduled for next boot.");
}

void resetParametersToDefaults(RobotParameters& params) {
    params.magic = PARAMS_MAGIC;
    params.isCalibrated = 0;

    for (int i = 0; i < NUM_SENSORS; i++) {
        params.sensorPins[i] = SENSOR_PINS[i];
        params.minVal[i] = DEFAULT_RAW_MIN;
        params.maxVal[i] = DEFAULT_RAW_MAX;
        params.thresh[i] = (DEFAULT_RAW_MIN + DEFAULT_RAW_MAX) / 2;
    }

    params.kp = DEFAULT_KP;
    params.ki = DEFAULT_KI;
    params.kd = DEFAULT_KD;

    params.baseSpeed = DEFAULT_BASE_SPEED;
    params.minSpeed = DEFAULT_MIN_SPEED;
    params.maxSpeed = DEFAULT_MAX_SPEED;

    params.invertLeftMotor = 0;
    params.invertRightMotor = 0;
    params.swapMotors = 0;

    params.invertLine = 0; // Black line on white surface default
    params.navMode = DEFAULT_NAV_MODE;
    strncpy(params.customPath, DEFAULT_RULEBOOK_ROUTE, sizeof(params.customPath) - 1);
    params.customPath[sizeof(params.customPath) - 1] = '\0';

    Serial.println("[NVS] Parameters reset to factory defaults (5 IR sensors, LFR9)");
}

bool isCalibrationValid(const RobotParameters& params) {
    if (params.magic != PARAMS_MAGIC) return false;
    if (params.isCalibrated == 0) return false;
    
    // Check reasonable difference between min and max on each sensor
    for (int i = 0; i < NUM_SENSORS; i++) {
        if (params.maxVal[i] <= params.minVal[i] + 150) {
            return false;
        }
    }
    
    return true;
}

bool loadParameters(RobotParameters& params) {
    size_t len = sizeof(RobotParameters);
    if (!s_prefs.isKey("params9")) {
        Serial.println("[NVS] No v3.0 (params9) parameters found. Initializing defaults...");
        resetParametersToDefaults(params);
        saveParameters(params);
        return false;
    }

    size_t readBytes = s_prefs.getBytes("params9", &params, len);
    if (readBytes != len || params.magic != PARAMS_MAGIC) {
        Serial.println("[NVS] Parameters corrupted or invalid magic. Resetting...");
        resetParametersToDefaults(params);
        saveParameters(params);
        return false;
    }

    Serial.printf("[NVS] Loaded 5-IR parameters. Calibrated=%u, NavMode=%u, Route='%s', Kp=%.2f, Ki=%.3f, Kd=%.2f, BaseSpeed=%d\n",
                  params.isCalibrated, params.navMode, params.customPath, params.kp, params.ki, params.kd, params.baseSpeed);
    Serial.printf("[NVS] Sensor Pins [S0-S4]: %u, %u, %u (Center), %u, %u\n",
                  params.sensorPins[0], params.sensorPins[1], params.sensorPins[2], params.sensorPins[3], params.sensorPins[4]);
    Serial.printf("[NVS] Motor Polarity: InvL=%u, InvR=%u, SwapLR=%u\n",
                  params.invertLeftMotor, params.invertRightMotor, params.swapMotors);
    Serial.print("[NVS] Thresholds [S0-S4]: ");
    for (int i = 0; i < NUM_SENSORS; i++) {
        Serial.printf("%u ", params.thresh[i]);
    }
    Serial.println();
    return true;
}

bool saveParameters(const RobotParameters& params) {
    size_t len = sizeof(RobotParameters);
    size_t written = s_prefs.putBytes("params9", &params, len);
    if (written == len) {
        Serial.println("[NVS] Parameters successfully saved to NVS (params9).");
        return true;
    } else {
        Serial.printf("[NVS] ERROR saving parameters! Written: %u / Expected: %u\n", (unsigned int)written, (unsigned int)len);
        return false;
    }
}

void startCalibration(uint32_t durationMs) {
    s_calibDuration = durationMs;
    s_calibStartTime = millis();
    s_calibrating = true;

    // Seed min/max for all 5 sensors
    for (int i = 0; i < NUM_SENSORS; i++) {
        s_sweepMin[i] = 4095;
        s_sweepMax[i] = 0;
    }

    Serial.printf("[CALIB] Started 5-sensor calibration sweep for %u ms...\n", (unsigned int)durationMs);
}

void updateCalibration(const uint16_t raw[NUM_SENSORS]) {
    if (!s_calibrating) return;

    for (int i = 0; i < NUM_SENSORS; i++) {
        if (raw[i] < s_sweepMin[i]) s_sweepMin[i] = raw[i];
        if (raw[i] > s_sweepMax[i]) s_sweepMax[i] = raw[i];
    }

    if (millis() - s_calibStartTime >= s_calibDuration) {
        applyCalibrationSweep(g_params);
        s_calibrating = false;
        Serial.println("[CALIB] 5-sensor calibration sweep completed and applied.");
    }
}

bool isCalibrating() {
    return s_calibrating;
}

uint32_t getCalibrationRemainingMs() {
    if (!s_calibrating) return 0;
    uint32_t elapsed = millis() - s_calibStartTime;
    if (elapsed >= s_calibDuration) return 0;
    return s_calibDuration - elapsed;
}

uint8_t getCalibrationProgressPct() {
    if (!s_calibrating) return 100;
    uint32_t elapsed = millis() - s_calibStartTime;
    if (elapsed >= s_calibDuration) return 100;
    return (uint8_t)((elapsed * 100) / s_calibDuration);
}

void getCalibrationSweepValues(uint16_t minVals[NUM_SENSORS], uint16_t maxVals[NUM_SENSORS]) {
    for (int i = 0; i < NUM_SENSORS; i++) {
        minVals[i] = s_sweepMin[i];
        maxVals[i] = s_sweepMax[i];
    }
}

void applyCalibrationSweep(RobotParameters& params) {
    for (int i = 0; i < NUM_SENSORS; i++) {
        if (s_sweepMax[i] > s_sweepMin[i]) {
            params.minVal[i] = s_sweepMin[i];
            params.maxVal[i] = s_sweepMax[i];
            params.thresh[i] = (s_sweepMin[i] + s_sweepMax[i]) / 2;
        }
    }

    params.isCalibrated = 1;
    saveParameters(params);
    g_pid.updateParams(params);
}

void cancelCalibration() {
    s_calibrating = false;
    Serial.println("[CALIB] Calibration cancelled.");
}

// ----------------------------------------------------------------------------
//  Sensor Order Auto-Detection
// ----------------------------------------------------------------------------
static bool s_detectingOrder = false;
static uint32_t s_detectStartTime = 0;
static uint8_t s_detectedOrder[NUM_SENSORS] = {0};
static uint8_t s_detectedCount = 0;
static const uint8_t CANDIDATE_PINS[5] = {33, 32, 35, 34, 39};

void startSensorDetection() {
    s_detectingOrder = true;
    s_detectStartTime = millis();
    s_detectedCount = 0;
    for (int i = 0; i < NUM_SENSORS; i++) {
        s_detectedOrder[i] = 0;
    }
    Serial.println("[CALIB] Sensor order auto-detection started! Slowly slide robot from LEFT to RIGHT over the black line...");
}

bool updateSensorDetection() {
    if (!s_detectingOrder) return false;

    if (millis() - s_detectStartTime > 10000) {
        s_detectingOrder = false;
        Serial.println("[CALIB] Sensor auto-detection timed out.");
        return false;
    }

    for (int c = 0; c < 5; c++) {
        uint8_t pin = CANDIDATE_PINS[c];
        uint16_t val = readSensorRaw(pin);

        // Strong black detection threshold
        if (val > 1500) {
            bool alreadyAdded = false;
            for (int k = 0; k < s_detectedCount; k++) {
                if (s_detectedOrder[k] == pin) {
                    alreadyAdded = true;
                    break;
                }
            }
            if (!alreadyAdded && s_detectedCount < NUM_SENSORS) {
                s_detectedOrder[s_detectedCount] = pin;
                Serial.printf("[CALIB] Order Slot S%u -> GPIO %u (raw=%u)\n", s_detectedCount, pin, val);
                s_detectedCount++;
            }
        }
    }

    if (s_detectedCount >= NUM_SENSORS) {
        s_detectingOrder = false;
        for (int i = 0; i < NUM_SENSORS; i++) {
            g_params.sensorPins[i] = s_detectedOrder[i];
        }
        saveParameters(g_params);
        g_pid.updateParams(g_params);
        Serial.printf("[CALIB] Sensor order successfully auto-detected: [%u, %u, %u, %u, %u]\n",
                      g_params.sensorPins[0], g_params.sensorPins[1], g_params.sensorPins[2],
                      g_params.sensorPins[3], g_params.sensorPins[4]);
        return true;
    }

    return false;
}

bool isDetectingSensors() {
    return s_detectingOrder;
}

void getDetectedPins(uint8_t pins[NUM_SENSORS]) {
    for (int i = 0; i < NUM_SENSORS; i++) {
        pins[i] = s_detectedOrder[i];
    }
}

