#pragma once

#include <Arduino.h>
#include "config.h"

// Magic number to verify valid parameter block in NVS.
// Bump whenever RobotParameters layout changes to force re-calibration.
#define PARAMS_MAGIC 0x4C465234 // "LFR4" — v1.1: 9-sensor array

// ---------------------------------------------------------------------------
//  Per-sensor calibration data (min / max / threshold)
// ---------------------------------------------------------------------------
struct SensorCal {
    uint16_t minVal;
    uint16_t maxVal;
    uint16_t thresh;    // midpoint; can be manually overridden via portal
};

// ---------------------------------------------------------------------------
//  Complete robot parameter block persisted in NVS
// ---------------------------------------------------------------------------
struct RobotParameters {
    uint32_t  magic;
    uint8_t   isCalibrated;

    // 9-sensor calibration data (S0..S8, left to right)
    // During calibration mode (WiFi on) only S0-S5 (ADC1) are updated.
    // S6-S8 (ADC2) are updated separately once ADC2 is usable (running mode).
    SensorCal sensors[NUM_SENSORS];

    // PID tuning
    float kp;
    float ki;
    float kd;

    // Motor speeds
    int16_t baseSpeed;
    int16_t minSpeed;
    int16_t maxSpeed;

    // 0 = Black line on white surface, 1 = White line on black surface
    uint8_t invertLine;

    // Navigation Strategy:
    // 0 = Autonomous LSRB (Left-Hand Rule) - default for unknown/changed maps
    // 1 = Autonomous RSLB (Right-Hand Rule)
    // 2 = Custom / Pre-Programmed Path Sequence
    // 3 = Standard PID Line Follower
    uint8_t navMode;

    // Custom Path string (e.g. "RRLLRLSSLLRRLL") for pre-programming any changed map
    char customPath[MAX_PATH_STEPS];
};

// Global parameters instance
extern RobotParameters g_params;

// ---------------------------------------------------------------------------
//  NVS & Boot counter functions
// ---------------------------------------------------------------------------
void initPreferences();
uint32_t getBootCount();
uint32_t incrementBootCount();
void resetBootCount();
void forceCalibrationNextBoot();

// ---------------------------------------------------------------------------
//  Parameter management
// ---------------------------------------------------------------------------
bool loadParameters(RobotParameters& params);
bool saveParameters(const RobotParameters& params);
void resetParametersToDefaults(RobotParameters& params);
bool isCalibrationValid(const RobotParameters& params);

// ---------------------------------------------------------------------------
//  Interactive Calibration Sweep state machine
//
//  During MODE_CALIBRATION (WiFi active) only ADC1 sensors (indices 0-5) are
//  swept.  ADC2 sensors (indices 6-8) are excluded.  Their calibration data is
//  copied from the nearest ADC1 neighbour as a safe default and can be refined
//  through the web portal's manual threshold sliders.
//
//  Call:
//    startCalibration()          — resets sweep accumulators, sets timer
//    updateCalibration(raw[9])   — feed a 9-element raw reading array each tick
//    applyCalibrationSweep()     — writes results into g_params and saves NVS
// ---------------------------------------------------------------------------
void startCalibration(uint32_t durationMs = CALIBRATION_DURATION_MS);

// raw[NUM_SENSORS]: pass the current raw ADC reading for every sensor.
// Entries for ADC2 sensors (index 6-8) should be 0 during calibration mode —
// the function will skip them automatically.
void updateCalibration(const uint16_t raw[NUM_SENSORS], bool adc2Available);

bool     isCalibrating();
uint32_t getCalibrationRemainingMs();
uint8_t  getCalibrationProgressPct();

// Returns per-sensor sweep min/max accumulated so far
void getCalibrationSweepValues(uint16_t minOut[NUM_SENSORS],
                               uint16_t maxOut[NUM_SENSORS]);

void applyCalibrationSweep(RobotParameters& params);
void cancelCalibration();
