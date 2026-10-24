#include "calibration.h"
#include <Preferences.h>

RobotParameters g_params;

static Preferences s_prefs;
static bool     s_calibrating    = false;
static uint32_t s_calibStartTime = 0;
static uint32_t s_calibDuration  = CALIBRATION_DURATION_MS;

// Per-sensor sweep accumulators
static uint16_t s_sweepMin[NUM_SENSORS];
static uint16_t s_sweepMax[NUM_SENSORS];

// ---------------------------------------------------------------------------
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
    // Set to (interval - 1) so next boot increments to the trigger value
    s_prefs.putUInt("boot_count", CALIBRATION_BOOT_INTERVAL - 1);
    Serial.println("[NVS] Calibration scheduled for next boot.");
}

// ---------------------------------------------------------------------------
void resetParametersToDefaults(RobotParameters& params) {
    params.magic        = PARAMS_MAGIC;
    params.isCalibrated = 0;

    for (int i = 0; i < NUM_SENSORS; i++) {
        params.sensors[i].minVal = DEFAULT_RAW_MIN;
        params.sensors[i].maxVal = DEFAULT_RAW_MAX;
        params.sensors[i].thresh = (DEFAULT_RAW_MIN + DEFAULT_RAW_MAX) / 2;
    }

    params.kp        = DEFAULT_KP;
    params.ki        = DEFAULT_KI;
    params.kd        = DEFAULT_KD;

    params.baseSpeed = DEFAULT_BASE_SPEED;
    params.minSpeed  = DEFAULT_MIN_SPEED;
    params.maxSpeed  = DEFAULT_MAX_SPEED;

    params.invertLine = 0;  // Black line on white surface
    params.navMode    = DEFAULT_NAV_MODE;
    strncpy(params.customPath, DEFAULT_RULEBOOK_ROUTE, sizeof(params.customPath) - 1);
    params.customPath[sizeof(params.customPath) - 1] = '\0';

    Serial.println("[NVS] Parameters reset to factory defaults");
}

// ---------------------------------------------------------------------------
bool isCalibrationValid(const RobotParameters& params) {
    if (params.magic != PARAMS_MAGIC) return false;
    if (params.isCalibrated == 0)     return false;

    // Check that the ADC1 sensors (S0-S5) have a meaningful range
    for (int i = 0; i < ADC1_SENSOR_COUNT; i++) {
        if (params.sensors[i].maxVal <= params.sensors[i].minVal + 150) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
bool loadParameters(RobotParameters& params) {
    size_t len = sizeof(RobotParameters);
    if (!s_prefs.isKey("params")) {
        Serial.println("[NVS] No parameters found. Initialising defaults...");
        resetParametersToDefaults(params);
        saveParameters(params);
        return false;
    }

    size_t readBytes = s_prefs.getBytes("params", &params, len);
    if (readBytes != len || params.magic != PARAMS_MAGIC) {
        Serial.println("[NVS] Parameters corrupted or wrong magic. Resetting...");
        resetParametersToDefaults(params);
        saveParameters(params);
        return false;
    }

    Serial.printf("[NVS] Loaded. Calibrated=%u NavMode=%u Route='%s' Kp=%.2f Ki=%.3f Kd=%.2f Base=%d\n",
                  params.isCalibrated, params.navMode, params.customPath,
                  params.kp, params.ki, params.kd, params.baseSpeed);
    for (int i = 0; i < NUM_SENSORS; i++) {
        Serial.printf("[NVS]   S%d pin=%-2d min=%-4u max=%-4u thresh=%-4u  (%s)\n",
                      i, 0 /*pin lookup done in config.h*/,
                      params.sensors[i].minVal,
                      params.sensors[i].maxVal,
                      params.sensors[i].thresh,
                      i < ADC1_SENSOR_COUNT ? "ADC1-WiFi-safe" : "ADC2-run-only");
    }
    return true;
}

// ---------------------------------------------------------------------------
bool saveParameters(const RobotParameters& params) {
    size_t len     = sizeof(RobotParameters);
    size_t written = s_prefs.putBytes("params", &params, len);
    if (written == len) {
        Serial.println("[NVS] Parameters saved.");
        return true;
    }
    Serial.printf("[NVS] ERROR saving parameters! written=%u expected=%u\n",
                  (unsigned)written, (unsigned)len);
    return false;
}

// ---------------------------------------------------------------------------
//  Calibration Sweep state machine
// ---------------------------------------------------------------------------
void startCalibration(uint32_t durationMs) {
    s_calibDuration  = durationMs;
    s_calibStartTime = millis();
    s_calibrating    = true;

    for (int i = 0; i < NUM_SENSORS; i++) {
        s_sweepMin[i] = 4095;
        s_sweepMax[i] = 0;
    }

    Serial.printf("[CALIB] Sweep started. Duration=%u ms. ADC2 sensors (S6-S8) "
                  "excluded during WiFi-active calibration.\n", (unsigned)durationMs);
}

// raw[NUM_SENSORS]: caller provides readings for every sensor each tick.
// adc2Available=false  => skip indices 6-8 (WiFi active, ADC2 unusable).
// adc2Available=true   => include all 9 (running mode or WiFi disabled).
void updateCalibration(const uint16_t raw[NUM_SENSORS], bool adc2Available) {
    if (!s_calibrating) return;

    int limit = adc2Available ? NUM_SENSORS : ADC1_SENSOR_COUNT;

    for (int i = 0; i < limit; i++) {
        if (raw[i] < s_sweepMin[i]) s_sweepMin[i] = raw[i];
        if (raw[i] > s_sweepMax[i]) s_sweepMax[i] = raw[i];
    }

    if (millis() - s_calibStartTime >= s_calibDuration) {
        applyCalibrationSweep(g_params);
        s_calibrating = false;
        Serial.println("[CALIB] Sweep complete.");
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

void getCalibrationSweepValues(uint16_t minOut[NUM_SENSORS],
                               uint16_t maxOut[NUM_SENSORS]) {
    for (int i = 0; i < NUM_SENSORS; i++) {
        minOut[i] = s_sweepMin[i];
        maxOut[i] = s_sweepMax[i];
    }
}

// ---------------------------------------------------------------------------
//  Apply sweep results into params.
//  ADC2 sensors (S6-S8): if not swept, copy from the nearest ADC1 neighbour
//  (S5) as a safe default so the robot can still run.
// ---------------------------------------------------------------------------
void applyCalibrationSweep(RobotParameters& params) {
    for (int i = 0; i < NUM_SENSORS; i++) {
        if (s_sweepMax[i] > s_sweepMin[i]) {
            // Sensor was swept successfully
            params.sensors[i].minVal = s_sweepMin[i];
            params.sensors[i].maxVal = s_sweepMax[i];
            params.sensors[i].thresh = (s_sweepMin[i] + s_sweepMax[i]) / 2;
            Serial.printf("[CALIB] S%d: min=%u max=%u thresh=%u\n",
                          i, s_sweepMin[i], s_sweepMax[i],
                          params.sensors[i].thresh);
        } else if (i >= ADC1_SENSOR_COUNT) {
            // ADC2 sensor — not swept. Mirror from S5 (last ADC1 sensor).
            params.sensors[i].minVal = params.sensors[ADC1_SENSOR_COUNT - 1].minVal;
            params.sensors[i].maxVal = params.sensors[ADC1_SENSOR_COUNT - 1].maxVal;
            params.sensors[i].thresh = params.sensors[ADC1_SENSOR_COUNT - 1].thresh;
            Serial.printf("[CALIB] S%d (ADC2, not swept): mirrored from S%d\n",
                          i, ADC1_SENSOR_COUNT - 1);
        }
    }

    params.isCalibrated = 1;
    saveParameters(params);
}

void cancelCalibration() {
    s_calibrating = false;
    Serial.println("[CALIB] Calibration cancelled.");
}
