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
    bool onLine[NUM_SENSORS];
    
    int16_t position;      // -4000 (far left) to +4000 (far right), 0 = center
    LineStatus status;
    uint8_t checkpoints;   // Count of checkpoints passed (up to 4)
    uint8_t junctionIndex; // Progress in custom/map turn sequence
    bool runFinished;
    char solvedPath[MAX_PATH_STEPS]; // Simplified shortest path discovered
};

class PIDController {
public:
    PIDController();

    void init(const RobotParameters& params);
    void updateParams(const RobotParameters& params);
    void reset();

    // Read sensors and compute motor speeds (leftSpeed, rightSpeed)
    void compute(const uint16_t raw[NUM_SENSORS],
                 int16_t& outLeftSpeed, int16_t& outRightSpeed);

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
