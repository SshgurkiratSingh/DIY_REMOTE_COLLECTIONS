#include "calibration.h"
#include <Preferences.h>

RobotParameters g_params;

static Preferences s_prefs;
static bool s_calibrating = false;
static uint32_t s_calibStartTime = 0;
static uint32_t s_calibDuration = CALIBRATION_DURATION_MS;

static uint16_t s_sweepMinL = 4095;
static uint16_t s_sweepMaxL = 0;
static uint16_t s_sweepMinC = 4095;
static uint16_t s_sweepMaxC = 0;
static uint16_t s_sweepMinR = 4095;
static uint16_t s_sweepMaxR = 0;

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

    params.minL = DEFAULT_RAW_MIN;
    params.maxL = DEFAULT_RAW_MAX;
    params.threshL = (DEFAULT_RAW_MIN + DEFAULT_RAW_MAX) / 2;

    params.minC = DEFAULT_RAW_MIN;
    params.maxC = DEFAULT_RAW_MAX;
    params.threshC = (DEFAULT_RAW_MIN + DEFAULT_RAW_MAX) / 2;

    params.minR = DEFAULT_RAW_MIN;
    params.maxR = DEFAULT_RAW_MAX;
    params.threshR = (DEFAULT_RAW_MIN + DEFAULT_RAW_MAX) / 2;

    params.kp = DEFAULT_KP;
    params.ki = DEFAULT_KI;
    params.kd = DEFAULT_KD;

    params.baseSpeed = DEFAULT_BASE_SPEED;
    params.minSpeed = DEFAULT_MIN_SPEED;
    params.maxSpeed = DEFAULT_MAX_SPEED;

    params.invertLine = 0; // Black line on white surface default

    Serial.println("[NVS] Parameters reset to factory defaults");
}

bool isCalibrationValid(const RobotParameters& params) {
    if (params.magic != PARAMS_MAGIC) return false;
    if (params.isCalibrated == 0) return false;
    
    // Check reasonable difference between min and max
    if (params.maxL <= params.minL + 150) return false;
    if (params.maxC <= params.minC + 150) return false;
    if (params.maxR <= params.minR + 150) return false;
    
    return true;
}

bool loadParameters(RobotParameters& params) {
    size_t len = sizeof(RobotParameters);
    if (!s_prefs.isKey("params")) {
        Serial.println("[NVS] No parameters found. Initializing defaults...");
        resetParametersToDefaults(params);
        saveParameters(params);
        return false;
    }

    size_t readBytes = s_prefs.getBytes("params", &params, len);
    if (readBytes != len || params.magic != PARAMS_MAGIC) {
        Serial.println("[NVS] Parameters corrupted or invalid magic. Resetting...");
        resetParametersToDefaults(params);
        saveParameters(params);
        return false;
    }

    Serial.printf("[NVS] Loaded parameters. Calibrated=%u, Kp=%.2f, Ki=%.3f, Kd=%.2f, BaseSpeed=%d\n",
                  params.isCalibrated, params.kp, params.ki, params.kd, params.baseSpeed);
    Serial.printf("[NVS] Sensor L:[%u-%u, T:%u] C:[%u-%u, T:%u] R:[%u-%u, T:%u]\n",
                  params.minL, params.maxL, params.threshL,
                  params.minC, params.maxC, params.threshC,
                  params.minR, params.maxR, params.threshR);
    return true;
}

bool saveParameters(const RobotParameters& params) {
    size_t len = sizeof(RobotParameters);
    size_t written = s_prefs.putBytes("params", &params, len);
    if (written == len) {
        Serial.println("[NVS] Parameters successfully saved to NVS.");
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

    // Seed min/max
    s_sweepMinL = 4095;
    s_sweepMaxL = 0;
    s_sweepMinC = 4095;
    s_sweepMaxC = 0;
    s_sweepMinR = 4095;
    s_sweepMaxR = 0;

    Serial.printf("[CALIB] Started calibration sweep for %u ms...\n", (unsigned int)durationMs);
}

void updateCalibration(uint16_t rawL, uint16_t rawC, uint16_t rawR) {
    if (!s_calibrating) return;

    if (rawL < s_sweepMinL) s_sweepMinL = rawL;
    if (rawL > s_sweepMaxL) s_sweepMaxL = rawL;

    if (rawC < s_sweepMinC) s_sweepMinC = rawC;
    if (rawC > s_sweepMaxC) s_sweepMaxC = rawC;

    if (rawR < s_sweepMinR) s_sweepMinR = rawR;
    if (rawR > s_sweepMaxR) s_sweepMaxR = rawR;

    if (millis() - s_calibStartTime >= s_calibDuration) {
        applyCalibrationSweep(g_params);
        s_calibrating = false;
        Serial.println("[CALIB] Calibration sweep completed and applied.");
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

void getCalibrationSweepValues(uint16_t& minL, uint16_t& maxL,
                               uint16_t& minC, uint16_t& maxC,
                               uint16_t& minR, uint16_t& maxR) {
    minL = s_sweepMinL;
    maxL = s_sweepMaxL;
    minC = s_sweepMinC;
    maxC = s_sweepMaxC;
    minR = s_sweepMinR;
    maxR = s_sweepMaxR;
}

void applyCalibrationSweep(RobotParameters& params) {
    if (s_sweepMaxL > s_sweepMinL) {
        params.minL = s_sweepMinL;
        params.maxL = s_sweepMaxL;
        params.threshL = (s_sweepMinL + s_sweepMaxL) / 2;
    }
    if (s_sweepMaxC > s_sweepMinC) {
        params.minC = s_sweepMinC;
        params.maxC = s_sweepMaxC;
        params.threshC = (s_sweepMinC + s_sweepMaxC) / 2;
    }
    if (s_sweepMaxR > s_sweepMinR) {
        params.minR = s_sweepMinR;
        params.maxR = s_sweepMaxR;
        params.threshR = (s_sweepMinR + s_sweepMaxR) / 2;
    }

    params.isCalibrated = 1;
    saveParameters(params);
}

void cancelCalibration() {
    s_calibrating = false;
    Serial.println("[CALIB] Calibration cancelled.");
}
