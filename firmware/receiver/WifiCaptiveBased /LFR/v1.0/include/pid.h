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

    // Legacy 3-sensor convenience aliases (used by turn detection helpers)
    uint16_t rawL()  const { return raw[1];  }
    uint16_t rawC()  const { return raw[3];  }
    uint16_t rawR()  const { return raw[5];  }
    uint16_t normL() const { return norm[1]; }
    uint16_t normC() const { return norm[3]; }
    uint16_t normR() const { return norm[5]; }
    bool onLineL()   const { return onLine[1]; }
    bool onLineC()   const { return onLine[3]; }
    bool onLineR()   const { return onLine[5]; }
};

class PIDController {
public:
    PIDController();

    void init(const RobotParameters& params);
    void updateParams(const RobotParameters& params);
    void reset();

    // Read sensors and compute motor speeds.
    // raw[NUM_SENSORS]: raw ADC readings for all sensors.
    // activeSensors: how many sensors are valid (6 during calibration/WiFi-on, 9 during running).
    void compute(const uint16_t raw[NUM_SENSORS], uint8_t activeSensors,
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
