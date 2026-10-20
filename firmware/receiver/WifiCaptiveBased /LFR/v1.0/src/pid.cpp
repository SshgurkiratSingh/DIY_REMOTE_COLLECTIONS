#include "pid.h"
#include <math.h>

PIDController g_pid;

PIDController::PIDController()
    : m_lastError(0.0f),
      m_integral(0.0f),
      m_lastKnownPosition(0),
      m_lineLostStartTime(0),
      m_wasLineLost(false)
{
    memset(&m_params, 0, sizeof(m_params));
    memset(&m_readings, 0, sizeof(m_readings));
}

void PIDController::init(const RobotParameters& params) {
    updateParams(params);
    reset();
}

void PIDController::updateParams(const RobotParameters& params) {
    m_params = params;
}

void PIDController::reset() {
    m_lastError = 0.0f;
    m_integral = 0.0f;
    m_lastKnownPosition = 0;
    m_lineLostStartTime = 0;
    m_wasLineLost = false;
}

uint16_t PIDController::normalizeSensor(uint16_t raw, uint16_t minVal, uint16_t maxVal, bool invert) {
    if (maxVal <= minVal) {
        return 0; // Prevent divide by zero if not calibrated
    }

    int32_t norm;
    if (!invert) {
        // Standard: Black line has higher raw ADC value
        norm = (int32_t)(raw - minVal) * 1000 / (int32_t)(maxVal - minVal);
    } else {
        // Inverted: White line on black has lower raw ADC value
        norm = (int32_t)(maxVal - raw) * 1000 / (int32_t)(maxVal - minVal);
    }

    if (norm < 0) norm = 0;
    if (norm > 1000) norm = 1000;
    return (uint16_t)norm;
}

void PIDController::compute(uint16_t rawL, uint16_t rawC, uint16_t rawR,
                           int16_t& outLeftSpeed, int16_t& outRightSpeed) {
    bool invert = (m_params.invertLine != 0);

    // 1. Normalize sensor values to 0 - 1000
    uint16_t normL = normalizeSensor(rawL, m_params.minL, m_params.maxL, invert);
    uint16_t normC = normalizeSensor(rawC, m_params.minC, m_params.maxC, invert);
    uint16_t normR = normalizeSensor(rawR, m_params.minR, m_params.maxR, invert);

    // Save telemetry
    m_readings.rawL = rawL;
    m_readings.rawC = rawC;
    m_readings.rawR = rawR;
    m_readings.normL = normL;
    m_readings.normC = normC;
    m_readings.normR = normR;

    // Detect boolean state per sensor based on calibrated thresholds
    if (!invert) {
        m_readings.onLineL = (rawL >= m_params.threshL);
        m_readings.onLineC = (rawC >= m_params.threshC);
        m_readings.onLineR = (rawR >= m_params.threshR);
    } else {
        m_readings.onLineL = (rawL <= m_params.threshL);
        m_readings.onLineC = (rawC <= m_params.threshC);
        m_readings.onLineR = (rawR <= m_params.threshR);
    }

    uint32_t sum = (uint32_t)normL + (uint32_t)normC + (uint32_t)normR;

    // 2. Handle Edge Case: All sensors on black (Intersection / Wide Crossline)
    if (normL > 650 && normC > 650 && normR > 650) {
        m_readings.status = LINE_INTERSECTION;
        m_readings.position = 0;
        m_wasLineLost = false;
        m_integral = 0.0f; // Don't build integral error across intersections

        // Go straight at base speed
        outLeftSpeed = m_params.baseSpeed;
        outRightSpeed = m_params.baseSpeed;
        return;
    }

    // 3. Handle Edge Case: All sensors on white (Line Lost / Off track)
    if (sum < 200) {
        if (!m_wasLineLost) {
            m_wasLineLost = true;
            m_lineLostStartTime = millis();
        }

        uint32_t lostDuration = millis() - m_lineLostStartTime;

        if (lostDuration < LINE_LOST_TIMEOUT_MS) {
            // Stage 1: Fast Recovery turn in direction of last known position
            m_readings.status = LINE_RECOVERY;
            if (m_lastKnownPosition < 0) {
                // Line was to the left -> turn sharply left
                outLeftSpeed = -(m_params.baseSpeed / 2);
                outRightSpeed = m_params.baseSpeed;
            } else {
                // Line was to the right -> turn sharply right
                outLeftSpeed = m_params.baseSpeed;
                outRightSpeed = -(m_params.baseSpeed / 2);
            }
        } else if (lostDuration < 4000) {
            // Stage 2: Slow rotation search pattern
            m_readings.status = LINE_LOST_SEARCHING;
            int16_t searchSpeed = 75;
            if (m_lastKnownPosition < 0) {
                outLeftSpeed = -searchSpeed;
                outRightSpeed = searchSpeed;
            } else {
                outLeftSpeed = searchSpeed;
                outRightSpeed = -searchSpeed;
            }
        } else {
            // Stage 3: Complete stop after extended loss
            m_readings.status = LINE_LOST_STOPPED;
            outLeftSpeed = 0;
            outRightSpeed = 0;
        }
        return;
    }

    // Line is detected! Reset line lost state
    m_wasLineLost = false;

    // 4. Compute Weighted Position Error
    // Position = (-1000 * L + 0 * C + 1000 * R) / (L + C + R)
    int32_t weightedSum = (-1000LL * (int32_t)normL) + (1000LL * (int32_t)normR);
    int16_t position = (int16_t)(weightedSum / (int32_t)sum);
    m_readings.position = position;
    m_lastKnownPosition = position;

    // 5. Handle Edge Case: Sharp 90-degree turns
    if (normL > 700 && normC < 250 && normR < 250) {
        // Sharp Left
        m_readings.status = LINE_SHARP_LEFT;
        m_integral = 0.0f;
        outLeftSpeed = -(m_params.baseSpeed / 3);
        outRightSpeed = m_params.baseSpeed;
        return;
    }

    if (normR > 700 && normC < 250 && normL < 250) {
        // Sharp Right
        m_readings.status = LINE_SHARP_RIGHT;
        m_integral = 0.0f;
        outLeftSpeed = m_params.baseSpeed;
        outRightSpeed = -(m_params.baseSpeed / 3);
        return;
    }

    // 6. Normal PID Line Following
    m_readings.status = LINE_ON_TRACK;
    float error = (float)position;

    // Proportional
    float pTerm = m_params.kp * error;

    // Integral with anti-windup clamping to ±10000
    m_integral += m_params.ki * error;
    m_integral = constrain(m_integral, -PID_ANTI_WINDUP_LIMIT, PID_ANTI_WINDUP_LIMIT);

    // Derivative
    float dTerm = m_params.kd * (error - m_lastError);
    m_lastError = error;

    // Total correction scaled
    float correction = (pTerm + m_integral + dTerm) / PID_SCALE_DIVISOR;

    int16_t left = m_params.baseSpeed + (int16_t)roundf(correction);
    int16_t right = m_params.baseSpeed - (int16_t)roundf(correction);

    // Constrain to configured speed bounds
    outLeftSpeed = constrain(left, m_params.minSpeed, m_params.maxSpeed);
    outRightSpeed = constrain(right, m_params.minSpeed, m_params.maxSpeed);
}

SensorReadings PIDController::getReadings() const {
    return m_readings;
}

const char* PIDController::statusToString(LineStatus st) {
    switch (st) {
        case LINE_ON_TRACK:       return "Tracking Line";
        case LINE_INTERSECTION:   return "Intersection / Crossline";
        case LINE_SHARP_LEFT:     return "Sharp Left Turn";
        case LINE_SHARP_RIGHT:    return "Sharp Right Turn";
        case LINE_RECOVERY:       return "Off Track - Line Recovery";
        case LINE_LOST_SEARCHING: return "Line Lost - Searching";
        case LINE_LOST_STOPPED:   return "Line Lost - Stopped";
        default:                  return "Unknown";
    }
}
