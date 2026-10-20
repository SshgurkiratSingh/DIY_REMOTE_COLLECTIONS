#pragma once

#include <Arduino.h>
#include "config.h"

// Magic number to verify valid parameter block in NVS
#define PARAMS_MAGIC 0x4C465231 // "LFR1"

struct RobotParameters {
    uint32_t magic;
    uint8_t isCalibrated;
    
    // IR Sensor Min / Max / Threshold
    uint16_t minL;
    uint16_t maxL;
    uint16_t threshL;
    
    uint16_t minC;
    uint16_t maxC;
    uint16_t threshC;
    
    uint16_t minR;
    uint16_t maxR;
    uint16_t threshR;

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
void updateCalibration(uint16_t rawL, uint16_t rawC, uint16_t rawR);
bool isCalibrating();
uint32_t getCalibrationRemainingMs();
uint8_t getCalibrationProgressPct();
void getCalibrationSweepValues(uint16_t& minL, uint16_t& maxL,
                               uint16_t& minC, uint16_t& maxC,
                               uint16_t& minR, uint16_t& maxR);
void applyCalibrationSweep(RobotParameters& params);
void cancelCalibration();
