#pragma once

#include <Arduino.h>
#include "config.h"

// Magic number to verify valid parameter block in NVS ("LFR5")
#define PARAMS_MAGIC 0x4C465235 // "LFR5"

struct RobotParameters {
    uint32_t magic;
    uint8_t isCalibrated;
    
    // 5 IR Sensor Min / Max / Threshold
    uint16_t minVal[NUM_SENSORS];
    uint16_t maxVal[NUM_SENSORS];
    uint16_t thresh[NUM_SENSORS];

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

// NVS & Boot counter functions
void initPreferences();
uint32_t getBootCount();
uint32_t incrementBootCount();
void resetBootCount();
void forceCalibrationNextBoot();

// Parameter management
bool loadParameters(RobotParameters& params);
bool saveParameters(const RobotParameters& params);
void resetParametersToDefaults(RobotParameters& params);
bool isCalibrationValid(const RobotParameters& params);

// Interactive Calibration Sweep state machine
void startCalibration(uint32_t durationMs = CALIBRATION_DURATION_MS);
void updateCalibration(const uint16_t raw[NUM_SENSORS]);
bool isCalibrating();
uint32_t getCalibrationRemainingMs();
uint8_t getCalibrationProgressPct();
void getCalibrationSweepValues(uint16_t minVals[NUM_SENSORS], uint16_t maxVals[NUM_SENSORS]);
void applyCalibrationSweep(RobotParameters& params);
void cancelCalibration();
