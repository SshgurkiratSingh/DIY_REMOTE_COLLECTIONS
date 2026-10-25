#include "pid.h"
#include <math.h>
#include "motor.h"

PIDController g_pid;

// Sensor physical weights (left to right, matching SENSOR_WEIGHTS in config.h)
static const int32_t WEIGHTS[NUM_SENSORS] = SENSOR_WEIGHTS;

// GPIO pin lookup for each sensor index
static const uint8_t SENSOR_PINS[NUM_SENSORS] = {
    SENSOR_S0_PIN, SENSOR_S1_PIN, SENSOR_S2_PIN,
    SENSOR_S3_PIN, SENSOR_S4_PIN, SENSOR_S5_PIN,
    SENSOR_S6_PIN, SENSOR_S7_PIN, SENSOR_S8_PIN
};

// ---------------------------------------------------------------------------
PIDController::PIDController()
    : m_lastError(0.0f),
      m_integral(0.0f),
      m_lastKnownPosition(0),
      m_lineLostStartTime(0),
      m_wasLineLost(false),
      m_lastCheckpointTime(0),
      m_allBlackStartTime(0),
      m_allBlackCandidate(false),
      m_pathLength(0)
{
    memset(&m_params,   0, sizeof(m_params));
    memset(&m_readings, 0, sizeof(m_readings));
    memset(m_path,      0, sizeof(m_path));
}

void PIDController::init(const RobotParameters& params) {
    updateParams(params);
    reset();
}

void PIDController::updateParams(const RobotParameters& params) {
    m_params = params;
}

void PIDController::reset() {
    m_lastError           = 0.0f;
    m_integral            = 0.0f;
    m_lastKnownPosition   = 0;
    m_lineLostStartTime   = 0;
    m_wasLineLost         = false;
    m_lastCheckpointTime  = 0;
    m_allBlackStartTime   = 0;
    m_allBlackCandidate   = false;
    m_pathLength          = 0;

    memset(m_path, 0, sizeof(m_path));
    memset(m_readings.solvedPath, 0, sizeof(m_readings.solvedPath));
    m_readings.checkpoints  = 0;
    m_readings.junctionIndex = 0;
    m_readings.runFinished  = false;
    m_readings.status       = LINE_ON_TRACK;
}

// ---------------------------------------------------------------------------
uint16_t PIDController::normalizeSensor(uint16_t raw, uint16_t minVal, uint16_t maxVal, bool invert) {
    if (maxVal <= minVal) return 0;

    int32_t norm;
    if (!invert) {
        norm = (int32_t)(raw - minVal) * 1000 / (int32_t)(maxVal - minVal);
    } else {
        norm = (int32_t)(maxVal - raw) * 1000 / (int32_t)(maxVal - minVal);
    }
    if (norm < 0)    norm = 0;
    if (norm > 1000) norm = 1000;
    return (uint16_t)norm;
}

bool PIDController::isFinished() const {
    return m_readings.runFinished;
}

const char* PIDController::getSolvedPath() const {
    return m_readings.solvedPath;
}

// ---------------------------------------------------------------------------
void PIDController::logTurn(char turn) {
    if (m_pathLength >= MAX_PATH_STEPS - 1) return;
    m_path[m_pathLength++] = turn;
    m_path[m_pathLength]   = '\0';
    Serial.printf("[PATH LOG] Recorded '%c' (Path: %s)\n", turn, m_path);
    simplifyPath();
    strncpy(m_readings.solvedPath, m_path, sizeof(m_readings.solvedPath) - 1);
    m_readings.solvedPath[sizeof(m_readings.solvedPath) - 1] = '\0';
}

void PIDController::simplifyPath() {
    if (m_pathLength < 3) return;
    if (m_path[m_pathLength - 2] != 'B') return;

    char first  = m_path[m_pathLength - 3];
    char second = m_path[m_pathLength - 1];
    char replacement = 0;

    if      (first == 'L' && second == 'L') replacement = 'S';
    else if (first == 'L' && second == 'S') replacement = 'R';
    else if (first == 'R' && second == 'L') replacement = 'B';
    else if (first == 'S' && second == 'L') replacement = 'R';
    else if (first == 'S' && second == 'S') replacement = 'B';
    else if (first == 'L' && second == 'R') replacement = 'B';
    else if (first == 'R' && second == 'S') replacement = 'L';
    else if (first == 'R' && second == 'R') replacement = 'S';
    else if (first == 'S' && second == 'R') replacement = 'L';

    if (replacement != 0) {
        m_path[m_pathLength - 3] = replacement;
        m_pathLength -= 2;
        m_path[m_pathLength] = '\0';
        Serial.printf("[PATH SIMPLIFY] '%c B %c' -> '%c'  Path: %s\n",
                      first, second, replacement, m_path);
        simplifyPath(); // Recurse to catch chains
    }
}

// ---------------------------------------------------------------------------
TurnAction PIDController::decideTurnAtJunction(bool canLeft, bool canStraight, bool canRight) {
    if (m_params.navMode == NAV_MODE_CUSTOM_PATH) {
        size_t len = strlen(m_params.customPath);
        if (m_readings.junctionIndex < len) {
            char choice = m_params.customPath[m_readings.junctionIndex++];
            Serial.printf("[CUSTOM NAV] Junction #%u -> '%c'\n", m_readings.junctionIndex, choice);
            if (choice == 'L' || choice == 'l') return TURN_LEFT_90;
            if (choice == 'R' || choice == 'r') return TURN_RIGHT_90;
            if (choice == 'S' || choice == 's') return TURN_STRAIGHT;
            if (choice == 'B' || choice == 'b') return TURN_UTURN_180;
        }
    }

    if (m_params.navMode == NAV_MODE_AUTONOMOUS_RSLB) {
        if (canRight)    return TURN_RIGHT_90;
        if (canStraight) return TURN_STRAIGHT;
        if (canLeft)     return TURN_LEFT_90;
        return TURN_UTURN_180;
    }

    // Default: LSRB
    if (canLeft)     return TURN_LEFT_90;
    if (canStraight) return TURN_STRAIGHT;
    if (canRight)    return TURN_RIGHT_90;
    return TURN_UTURN_180;
}

// ---------------------------------------------------------------------------
void PIDController::executeTurn(TurnAction action) {
    if (action == TURN_NONE) return;

    if (action == TURN_STRAIGHT) {
        logTurn('S');
        setMotorSpeed(m_params.baseSpeed, m_params.baseSpeed);
        delay(JUNCTION_INCH_TIME_MS);
        notifyMotorSafetyFeed();
        return;
    }

    m_readings.status = LINE_TURNING;
    int16_t turnSpd   = TURN_SPEED;
    bool    invert    = (m_params.invertLine != 0);

    if (action == TURN_LEFT_90) {
        logTurn('L');
        Serial.println("[NAV] 90-deg LEFT turn...");
        setMotorSpeed(-turnSpd, turnSpd);
        delay(150);
        notifyMotorSafetyFeed();

        uint32_t start = millis();
        while (millis() - start < TURN_TIMEOUT_MS) {
            uint16_t rawC  = analogRead(SENSOR_CENTER_PIN);
            uint16_t normC = normalizeSensor(rawC,
                                             m_params.sensors[3].minVal,
                                             m_params.sensors[3].maxVal, invert);
            if (normC > 500) break;
            notifyMotorSafetyFeed();
            delay(5);
        }
    } else if (action == TURN_RIGHT_90) {
        logTurn('R');
        Serial.println("[NAV] 90-deg RIGHT turn...");
        setMotorSpeed(turnSpd, -turnSpd);
        delay(150);
        notifyMotorSafetyFeed();

        uint32_t start = millis();
        while (millis() - start < TURN_TIMEOUT_MS) {
            uint16_t rawC  = analogRead(SENSOR_CENTER_PIN);
            uint16_t normC = normalizeSensor(rawC,
                                             m_params.sensors[3].minVal,
                                             m_params.sensors[3].maxVal, invert);
            if (normC > 500) break;
            notifyMotorSafetyFeed();
            delay(5);
        }
    } else if (action == TURN_UTURN_180) {
        logTurn('B');
        Serial.println("[NAV] Dead end — 180-deg U-TURN...");
        setMotorSpeed(-turnSpd, turnSpd);
        delay(280);
        notifyMotorSafetyFeed();

        uint32_t start = millis();
        while (millis() - start < TURN_TIMEOUT_MS) {
            uint16_t rawC  = analogRead(SENSOR_CENTER_PIN);
            uint16_t normC = normalizeSensor(rawC,
                                             m_params.sensors[3].minVal,
                                             m_params.sensors[3].maxVal, invert);
            if (normC > 500) break;
            notifyMotorSafetyFeed();
            delay(5);
        }
    }

    setMotorSpeed(0, 0);
    delay(40);
    m_integral  = 0.0f;
    m_lastError = 0.0f;
    notifyMotorSafetyFeed();
}

// ---------------------------------------------------------------------------
//  compute() — 9-sensor weighted PID
//
//  raw[NUM_SENSORS]: ADC readings.
//  adc2Available: false when WiFi is active (only 6 ADC1 sensors active),
//                 true during running mode (all 9 sensors active).
// ---------------------------------------------------------------------------
void PIDController::compute(const uint16_t raw[NUM_SENSORS], bool adc2Available,
                            int16_t& outLeftSpeed, int16_t& outRightSpeed) {
    if (m_readings.runFinished) {
        outLeftSpeed  = 0;
        outRightSpeed = 0;
        return;
    }

    bool invert = (m_params.invertLine != 0);
    static const bool S_IS_ADC1[NUM_SENSORS] = SENSOR_IS_ADC1;

    // 1. Normalize all available sensors to 0-1000
    uint8_t activeCount = 0;
    for (uint8_t i = 0; i < NUM_SENSORS; i++) {
        if (adc2Available || S_IS_ADC1[i]) {
            activeCount++;
            m_readings.raw[i]  = raw[i];
            m_readings.norm[i] = normalizeSensor(raw[i],
                                                  m_params.sensors[i].minVal,
                                                  m_params.sensors[i].maxVal, invert);
            m_readings.onLine[i] = invert
                ? (raw[i] <= m_params.sensors[i].thresh)
                : (raw[i] >= m_params.sensors[i].thresh);
        } else {
            // Inactive ADC2 sensor (S0, S4, or S8) during WiFi calibration
            m_readings.raw[i]    = 0;
            m_readings.norm[i]   = 0;
            m_readings.onLine[i] = false;
        }
    }

    // Convenience references for junction/end-zone detection:
    // Left: S1 (-3000) | Center: S4 (0) or (S3+S5)/2 | Right: S7 (+3000)
    uint16_t normL = m_readings.normL();
    uint16_t normC = m_readings.normC();
    uint16_t normR = m_readings.normR();

    // ========================================================================
    //  Case 1: ALL ACTIVE SENSORS ON BLACK (Zone B End Square vs Checkpoint)
    // ========================================================================
    bool allBlack = true;
    for (uint8_t i = 0; i < NUM_SENSORS; i++) {
        if (adc2Available || S_IS_ADC1[i]) {
            if (m_readings.norm[i] <= 650) { allBlack = false; break; }
        }
    }

    if (allBlack) {
        if (!m_allBlackCandidate) {
            m_allBlackCandidate  = true;
            m_allBlackStartTime  = millis();
        }
        if (millis() - m_allBlackStartTime >= END_ZONE_INCH_MS) {
            m_readings.status      = LINE_END_ZONE;
            m_readings.runFinished = true;
            outLeftSpeed           = 0;
            outRightSpeed          = 0;
            Serial.println("\n***************************************************");
            Serial.println("   [ZONE B REACHED] End Point Detected! RUN COMPLETE!");
            Serial.printf( "   Checkpoints: %u / 4\n", m_readings.checkpoints);
            Serial.printf( "   Solved Route: %s\n",    m_readings.solvedPath);
            Serial.println("***************************************************\n");
            return;
        }
        outLeftSpeed  = m_params.baseSpeed / 2;
        outRightSpeed = m_params.baseSpeed / 2;
        return;
    }

    // Transverse line check (checkpoint)
    if (m_allBlackCandidate) {
        m_allBlackCandidate = false;
        if (millis() - m_lastCheckpointTime > CHECKPOINT_DEBOUNCE_MS) {
            m_lastCheckpointTime = millis();
            m_readings.checkpoints++;
            m_readings.status = LINE_CHECKPOINT;
            Serial.printf("[CHECKPOINT] Passed checkpoint #%u\n", m_readings.checkpoints);
        }
    }

    // ========================================================================
    //  Case 2: ALL ACTIVE SENSORS ON WHITE (line lost)
    // ========================================================================
    uint32_t sumNorm = 0;
    for (uint8_t i = 0; i < NUM_SENSORS; i++) {
        if (adc2Available || S_IS_ADC1[i]) {
            sumNorm += m_readings.norm[i];
        }
    }
    uint32_t threshold = 220UL * activeCount / NUM_SENSORS; // scale threshold

    if (sumNorm < threshold) {
        if (!m_wasLineLost) {
            m_wasLineLost       = true;
            m_lineLostStartTime = millis();
        }

        if (m_params.navMode != NAV_MODE_PID_LINE) {
            // Inch forward and re-check before declaring dead end
            delay(50);
            uint32_t recheckSum = 0;
            for (uint8_t i = 0; i < NUM_SENSORS; i++) {
                if (adc2Available || S_IS_ADC1[i]) {
                    uint16_t r    = analogRead(SENSOR_PINS[i]);
                    recheckSum   += normalizeSensor(r,
                                                    m_params.sensors[i].minVal,
                                                    m_params.sensors[i].maxVal, invert);
                }
            }
            if (recheckSum < threshold) {
                m_readings.status = LINE_DEAD_END;
                executeTurn(TURN_UTURN_180);
                outLeftSpeed  = m_params.baseSpeed;
                outRightSpeed = m_params.baseSpeed;
                m_wasLineLost = false;
                return;
            }
        }

        uint32_t lostDuration = millis() - m_lineLostStartTime;
        if (lostDuration < LINE_LOST_TIMEOUT_MS) {
            m_readings.status = LINE_RECOVERY;
            if (m_lastKnownPosition < 0) {
                outLeftSpeed  = -(m_params.baseSpeed / 2);
                outRightSpeed =   m_params.baseSpeed;
            } else {
                outLeftSpeed  =   m_params.baseSpeed;
                outRightSpeed = -(m_params.baseSpeed / 2);
            }
        } else {
            m_readings.status = LINE_LOST_STOPPED;
            outLeftSpeed  = 0;
            outRightSpeed = 0;
        }
        return;
    }

    m_wasLineLost = false;

    // ========================================================================
    //  Case 3: JUNCTIONS & T-JUNCTIONS (maze navigation using inner sensors)
    // ========================================================================
    if (m_params.navMode != NAV_MODE_PID_LINE) {
        if (normL > 650 && normC > 500 && normR < 350) {
            setMotorSpeed(m_params.baseSpeed, m_params.baseSpeed);
            delay(JUNCTION_INCH_TIME_MS);
            notifyMotorSafetyFeed();
            uint16_t afterC = normalizeSensor(analogRead(SENSOR_CENTER_PIN),
                                              m_params.sensors[3].minVal,
                                              m_params.sensors[3].maxVal, invert);
            TurnAction action = decideTurnAtJunction(true, afterC > 450, false);
            executeTurn(action);
            outLeftSpeed  = m_params.baseSpeed;
            outRightSpeed = m_params.baseSpeed;
            return;
        }

        if (normR > 650 && normC > 500 && normL < 350) {
            setMotorSpeed(m_params.baseSpeed, m_params.baseSpeed);
            delay(JUNCTION_INCH_TIME_MS);
            notifyMotorSafetyFeed();
            uint16_t afterC = normalizeSensor(analogRead(SENSOR_CENTER_PIN),
                                              m_params.sensors[3].minVal,
                                              m_params.sensors[3].maxVal, invert);
            TurnAction action = decideTurnAtJunction(false, afterC > 450, true);
            executeTurn(action);
            outLeftSpeed  = m_params.baseSpeed;
            outRightSpeed = m_params.baseSpeed;
            return;
        }
    }

    // ========================================================================
    //  Case 4: NORMAL PID — Weighted position across all active sensors
    //
    //  position = sum(WEIGHTS[i] * norm[i]) / sum(norm[i])
    //  Negative = left of line, positive = right of line.
    // ========================================================================
    int64_t weightedSum = 0;
    uint32_t totalNorm  = 0;
    for (uint8_t i = 0; i < NUM_SENSORS; i++) {
        if (adc2Available || S_IS_ADC1[i]) {
            weightedSum += (int64_t)WEIGHTS[i] * m_readings.norm[i];
            totalNorm   += m_readings.norm[i];
        }
    }

    int16_t position = (totalNorm > 0)
                       ? (int16_t)(weightedSum / (int64_t)totalNorm)
                       : m_lastKnownPosition;

    m_readings.position    = position;
    m_lastKnownPosition    = position;
    m_readings.status      = LINE_ON_TRACK;

    float error  = (float)position;
    float pTerm  = m_params.kp * error;
    m_integral  += m_params.ki * error;
    m_integral   = constrain(m_integral, -PID_ANTI_WINDUP_LIMIT, PID_ANTI_WINDUP_LIMIT);
    float dTerm  = m_params.kd * (error - m_lastError);
    m_lastError  = error;

    float correction = (pTerm + m_integral + dTerm) / PID_SCALE_DIVISOR;

    int16_t left  = m_params.baseSpeed + (int16_t)roundf(correction);
    int16_t right = m_params.baseSpeed - (int16_t)roundf(correction);

    outLeftSpeed  = constrain(left,  m_params.minSpeed, m_params.maxSpeed);
    outRightSpeed = constrain(right, m_params.minSpeed, m_params.maxSpeed);
}

// ---------------------------------------------------------------------------
SensorReadings PIDController::getReadings() const {
    return m_readings;
}

const char* PIDController::statusToString(LineStatus st) {
    switch (st) {
        case LINE_ON_TRACK:      return "Tracking";
        case LINE_INTERSECTION:  return "Intersection";
        case LINE_SHARP_LEFT:    return "Left Turn";
        case LINE_SHARP_RIGHT:   return "Right Turn";
        case LINE_DEAD_END:      return "Dead End";
        case LINE_CHECKPOINT:    return "Checkpoint";
        case LINE_END_ZONE:      return "Zone B Finish";
        case LINE_TURNING:       return "Turning";
        case LINE_RECOVERY:      return "Recovery";
        case LINE_LOST_STOPPED:  return "Stopped";
        case LINE_FINISHED:      return "Complete";
        default:                 return "Unknown";
    }
}
