#pragma once

#include <Arduino.h>
#include "config.h"
#include "calibration.h"

enum LineStatus {
    LINE_ON_TRACK = 0,
    LINE_INTERSECTION,      // 4-way, T-junction, or crossline
    LINE_SHARP_LEFT,        // 90-degree left turn
    LINE_SHARP_RIGHT,       // 90-degree right turn
    LINE_DEAD_END,          // Dead end (U-turn required)
    LINE_CHECKPOINT,        // Transverse checkpoint passed
    LINE_END_ZONE,          // Reached Zone B large black square!
    LINE_TURNING,           // Active pivot turn in progress
    LINE_RECOVERY,          // Line lost recovery
    LINE_LOST_STOPPED,      // Stopped after extended loss
    LINE_FINISHED           // Run complete and stopped
};

enum TurnAction {
    TURN_NONE = 0,
    TURN_LEFT_90,
    TURN_RIGHT_90,
    TURN_UTURN_180,
    TURN_STRAIGHT
};

struct SensorReadings {
    uint16_t raw[NUM_SENSORS];
    uint16_t norm[NUM_SENSORS];
    bool     onLine[NUM_SENSORS];

    int16_t    position;       // Weighted position: negative = left, positive = right
    LineStatus status;
    uint8_t    checkpoints;    // Count of checkpoints passed
    uint8_t    junctionIndex;  // Progress in custom/map turn sequence
    bool       runFinished;
    char       solvedPath[MAX_PATH_STEPS]; // Simplified shortest path discovered

    // 3-region convenience aliases (used by turn/junction detection)
    // Left: S0, S1, S2 | Center: S3, S4, S5 | Right: S6, S7, S8
    uint16_t rawL()  const { uint16_t m = (raw[0] > raw[1]) ? raw[0] : raw[1]; return (raw[2] > m) ? raw[2] : m; }
    uint16_t rawC()  const { uint16_t m = (raw[3] > raw[4]) ? raw[3] : raw[4]; return (raw[5] > m) ? raw[5] : m; }
    uint16_t rawR()  const { uint16_t m = (raw[6] > raw[7]) ? raw[6] : raw[7]; return (raw[8] > m) ? raw[8] : m; }
    uint16_t normL() const { uint16_t m = (norm[0] > norm[1]) ? norm[0] : norm[1]; return (norm[2] > m) ? norm[2] : m; }
    uint16_t normC() const { uint16_t m = (norm[3] > norm[4]) ? norm[3] : norm[4]; return (norm[5] > m) ? norm[5] : m; }
    uint16_t normR() const { uint16_t m = (norm[6] > norm[7]) ? norm[6] : norm[7]; return (norm[8] > m) ? norm[8] : m; }
    bool onLineL()   const { return onLine[0] || onLine[1] || onLine[2]; }
    bool onLineC()   const { return onLine[3] || onLine[4] || onLine[5]; }
    bool onLineR()   const { return onLine[6] || onLine[7] || onLine[8]; }
};

class PIDController {
public:
    PIDController();

    void init(const RobotParameters& params);
    void updateParams(const RobotParameters& params);
    void reset();

    // Read sensors and compute motor speeds.
    // raw[NUM_SENSORS]: raw ADC readings for all sensors.
    // adc2Available: false during WiFi on (only 6 ADC1 sensors active), true during running mode (all 9 active).
    void compute(const uint16_t raw[NUM_SENSORS], bool adc2Available,
                 int16_t& outLeftSpeed, int16_t& outRightSpeed);

    // Fast, non-blocking telemetry update for portal/monitoring (does NOT drive motors, execute turns, or block)
    void updateSensorsOnly(const uint16_t raw[NUM_SENSORS], bool adc2Available);

    // Normalize raw reading based on calibration parameters
    uint16_t normalizeSensor(uint16_t raw, uint16_t minVal, uint16_t maxVal, bool invert);

    // Get current telemetry / readings snapshot
    SensorReadings getReadings() const;

    // Check if run is finished (Zone B reached)
    bool isFinished() const;

    // Execute in-place turn (blocking helper for clean 90-deg or 180-deg turns)
    void executeTurn(TurnAction action);

    // Path simplification (Tremaux / LSRB dead-end cancellation)
    void logTurn(char turn);
    void simplifyPath();
    const char* getSolvedPath() const;

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
    uint32_t m_lastCheckpointTime;
    uint32_t m_allBlackStartTime;
    bool m_allBlackCandidate;

    // Dynamic path memory
    char m_path[MAX_PATH_STEPS];
    uint8_t m_pathLength;

    // Navigation decision helper
    TurnAction decideTurnAtJunction(bool canLeft, bool canStraight, bool canRight);
};

extern PIDController g_pid;
