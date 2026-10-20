#pragma once

#include <Arduino.h>
#include "config.h"
#include "calibration.h"

enum LineStatus {
    LINE_ON_TRACK = 0,
    LINE_INTERSECTION,
    LINE_SHARP_LEFT,
    LINE_SHARP_RIGHT,
    LINE_RECOVERY,
    LINE_LOST_SEARCHING,
    LINE_LOST_STOPPED
};

struct SensorReadings {
    uint16_t rawL;
    uint16_t rawC;
    uint16_t rawR;
    
    uint16_t normL;
    uint16_t normC;
    uint16_t normR;
    
    bool onLineL;
    bool onLineC;
    bool onLineR;
    
    int16_t position;      // -1000 (far left) to +1000 (far right)
    LineStatus status;
};

class PIDController {
public:
    PIDController();

    void init(const RobotParameters& params);
    void updateParams(const RobotParameters& params);
    void reset();

    // Read sensors and compute motor speeds (leftSpeed, rightSpeed)
    void compute(uint16_t rawL, uint16_t rawC, uint16_t rawR,
                 int16_t& outLeftSpeed, int16_t& outRightSpeed);

    // Normalize raw reading based on calibration parameters
    uint16_t normalizeSensor(uint16_t raw, uint16_t minVal, uint16_t maxVal, bool invert);

    // Get current telemetry / readings snapshot
    SensorReadings getReadings() const;

    // Helper to get string description of line status
    static const char* statusToString(LineStatus st);

private:
    RobotParameters m_params;
    SensorReadings  m_readings;

    float m_lastError;
    float m_integral;
    int16_t m_lastKnownPosition;
    
    uint32_t m_lineLostStartTime;
    bool m_wasLineLost;
};

extern PIDController g_pid;
