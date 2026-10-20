#include "pid.h"
#include <math.h>
#include "motor.h"

PIDController g_pid;

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
    memset(&m_params, 0, sizeof(m_params));
    memset(&m_readings, 0, sizeof(m_readings));
    memset(m_path, 0, sizeof(m_path));
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
    m_lastCheckpointTime = 0;
    m_allBlackStartTime = 0;
    m_allBlackCandidate = false;

    m_pathLength = 0;
    memset(m_path, 0, sizeof(m_path));
    memset(m_readings.solvedPath, 0, sizeof(m_readings.solvedPath));

    m_readings.checkpoints = 0;
    m_readings.junctionIndex = 0;
    m_readings.runFinished = false;
    m_readings.status = LINE_ON_TRACK;
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

bool PIDController::isFinished() const {
    return m_readings.runFinished;
}

const char* PIDController::getSolvedPath() const {
    return m_readings.solvedPath;
}

void PIDController::logTurn(char turn) {
    if (m_pathLength >= MAX_PATH_STEPS - 1) return;

    m_path[m_pathLength++] = turn;
    m_path[m_pathLength] = '\0';
    Serial.printf("[PATH LOG] Recorded turn: '%c' (Total: %s)\n", turn, m_path);

    simplifyPath();
    strncpy(m_readings.solvedPath, m_path, sizeof(m_readings.solvedPath) - 1);
    m_readings.solvedPath[sizeof(m_readings.solvedPath) - 1] = '\0';
}

void PIDController::simplifyPath() {
    // Need at least 3 turns to simplify: [X, B, Y]
    if (m_pathLength < 3) return;
    if (m_path[m_pathLength - 2] != 'B') return;

    char first = m_path[m_pathLength - 3];
    char second = m_path[m_pathLength - 1];
    char replacement = 0;

    // Standard LSRB Substitution Rules
    if (first == 'L' && second == 'L') replacement = 'S';
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
        Serial.printf("[PATH SIMPLIFY] Substituted '%c B %c' -> '%c'. New Path: %s\n",
                      first, second, replacement, m_path);

        // Recursive check in case new turn also ends in 'B'
        simplifyPath();
    }
}

TurnAction PIDController::decideTurnAtJunction(bool canLeft, bool canStraight, bool canRight) {
    // Mode 2: Custom / Pre-Programmed Route Sequence (for specified or user-entered maps)
    if (m_params.navMode == NAV_MODE_CUSTOM_PATH) {
        size_t len = strlen(m_params.customPath);
        if (m_readings.junctionIndex < len) {
            char choice = m_params.customPath[m_readings.junctionIndex++];
            Serial.printf("[CUSTOM NAV] Junction #%u -> Turn '%c'\n", m_readings.junctionIndex, choice);
            if (choice == 'L' || choice == 'l') return TURN_LEFT_90;
            if (choice == 'R' || choice == 'r') return TURN_RIGHT_90;
            if (choice == 'S' || choice == 's') return TURN_STRAIGHT;
            if (choice == 'B' || choice == 'b') return TURN_UTURN_180;
        }
        // Past end of sequence: fallback to Autonomous Left-Hand Rule
    }

    // Mode 1: Autonomous Right-Hand Rule (RSLB)
    if (m_params.navMode == NAV_MODE_AUTONOMOUS_RSLB) {
        if (canRight)    return TURN_RIGHT_90;
        if (canStraight) return TURN_STRAIGHT;
        if (canLeft)     return TURN_LEFT_90;
        return TURN_UTURN_180;
    }

    // Mode 0: Autonomous Left-Hand Rule (LSRB) - Default for unknown/changed maps
    if (canLeft)     return TURN_LEFT_90;
    if (canStraight) return TURN_STRAIGHT;
    if (canRight)    return TURN_RIGHT_90;
    return TURN_UTURN_180;
}

void PIDController::executeTurn(TurnAction action) {
    if (action == TURN_NONE) return;

    if (action == TURN_STRAIGHT) {
        logTurn('S');
        // Inch straight through intersection to clear transverse line
        setMotorSpeed(m_params.baseSpeed, m_params.baseSpeed);
        delay(JUNCTION_INCH_TIME_MS);
        notifyMotorSafetyFeed();
        return;
    }

    m_readings.status = LINE_TURNING;
    int16_t turnSpd = TURN_SPEED;

    if (action == TURN_LEFT_90) {
        logTurn('L');
        Serial.println("[NAV] Executing 90-deg LEFT turn...");
        // Pivot left: Left motor reverse, Right motor forward
        setMotorSpeed(-turnSpd, turnSpd);
        delay(150); // Blind turn time to move off current line
        notifyMotorSafetyFeed();

        uint32_t start = millis();
        while (millis() - start < TURN_TIMEOUT_MS) {
            uint16_t rawC = analogRead(SENSOR_CENTER_PIN);
            uint16_t normC = normalizeSensor(rawC, m_params.minC, m_params.maxC, m_params.invertLine != 0);
            if (normC > 500) {
                break; // Center sensor re-acquired line
            }
            notifyMotorSafetyFeed();
            delay(5);
        }
    } else if (action == TURN_RIGHT_90) {
        logTurn('R');
        Serial.println("[NAV] Executing 90-deg RIGHT turn...");
        // Pivot right: Left motor forward, Right motor reverse
        setMotorSpeed(turnSpd, -turnSpd);
        delay(150);
        notifyMotorSafetyFeed();

        uint32_t start = millis();
        while (millis() - start < TURN_TIMEOUT_MS) {
            uint16_t rawC = analogRead(SENSOR_CENTER_PIN);
            uint16_t normC = normalizeSensor(rawC, m_params.minC, m_params.maxC, m_params.invertLine != 0);
            if (normC > 500) {
                break; // Center sensor re-acquired line
            }
            notifyMotorSafetyFeed();
            delay(5);
        }
    } else if (action == TURN_UTURN_180) {
        logTurn('B');
        Serial.println("[NAV] Dead end! Executing 180-deg U-TURN...");
        // Pivot around: Left motor reverse, Right motor forward
        setMotorSpeed(-turnSpd, turnSpd);
        delay(280); // Blind turn time
        notifyMotorSafetyFeed();

        uint32_t start = millis();
        while (millis() - start < TURN_TIMEOUT_MS) {
            uint16_t rawC = analogRead(SENSOR_CENTER_PIN);
            uint16_t normC = normalizeSensor(rawC, m_params.minC, m_params.maxC, m_params.invertLine != 0);
            if (normC > 500) {
                break; // Center sensor re-acquired line
            }
            notifyMotorSafetyFeed();
            delay(5);
        }
    }

    // Brief brake & reset integral
    setMotorSpeed(0, 0);
    delay(40);
    m_integral = 0.0f;
    m_lastError = 0.0f;
    notifyMotorSafetyFeed();
}

void PIDController::compute(uint16_t rawL, uint16_t rawC, uint16_t rawR,
                           int16_t& outLeftSpeed, int16_t& outRightSpeed) {
    if (m_readings.runFinished) {
        outLeftSpeed = 0;
        outRightSpeed = 0;
        return;
    }

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

    // ========================================================================
    //  Case 1: ALL SENSORS ON BLACK (Zone B End Square vs Checkpoint / Cross)
    // ========================================================================
    if (normL > 650 && normC > 650 && normR > 650) {
        if (!m_allBlackCandidate) {
            m_allBlackCandidate = true;
            m_allBlackStartTime = millis();
        }

        // Check if all sensors remain on black continuously
        if (millis() - m_allBlackStartTime >= END_ZONE_INCH_MS) {
            // Reached Zone B: Large Black Square!
            m_readings.status = LINE_END_ZONE;
            m_readings.runFinished = true;
            outLeftSpeed = 0;
            outRightSpeed = 0;
            Serial.println("\n***************************************************");
            Serial.println("   [ZONE B REACHED] End Point Detected! RUN COMPLETE!   ");
            Serial.printf("   Checkpoints logged: %u / 4\n", m_readings.checkpoints);
            Serial.printf("   Shortest Solved Route: %s\n", m_readings.solvedPath);
            Serial.println("***************************************************\n");
            return;
        }

        // Inch forward slightly to inspect whether this is a checkpoint line or the big end square
        outLeftSpeed = m_params.baseSpeed / 2;
        outRightSpeed = m_params.baseSpeed / 2;
        return;
    }

    // If we were checking an all-black zone and now one side cleared:
    if (m_allBlackCandidate) {
        m_allBlackCandidate = false;
        // This was a transverse line (checkpoint or intersection crossing)
        if (millis() - m_lastCheckpointTime > CHECKPOINT_DEBOUNCE_MS) {
            m_lastCheckpointTime = millis();
            m_readings.checkpoints++;
            m_readings.status = LINE_CHECKPOINT;
            Serial.printf("[CHECKPOINT] Passed checkpoint #%u (Transverse line)\n", m_readings.checkpoints);
        }
    }

    // ========================================================================
    //  Case 2: DEAD END (All sensors on white)
    // ========================================================================
    uint32_t sum = (uint32_t)normL + (uint32_t)normC + (uint32_t)normR;
    if (sum < 220) {
        if (!m_wasLineLost) {
            m_wasLineLost = true;
            m_lineLostStartTime = millis();
        }

        // In Maze Solving modes: Dead end requires 180° U-turn!
        if (m_params.navMode != NAV_MODE_PID_LINE) {
            // Inch forward tiny bit to confirm true dead end
            delay(50);
            uint16_t cL = normalizeSensor(analogRead(SENSOR_LEFT_PIN), m_params.minL, m_params.maxL, invert);
            uint16_t cC = normalizeSensor(analogRead(SENSOR_CENTER_PIN), m_params.minC, m_params.maxC, invert);
            uint16_t cR = normalizeSensor(analogRead(SENSOR_RIGHT_PIN), m_params.minR, m_params.maxR, invert);
            if (cL < 250 && cC < 250 && cR < 250) {
                m_readings.status = LINE_DEAD_END;
                executeTurn(TURN_UTURN_180);
                outLeftSpeed = m_params.baseSpeed;
                outRightSpeed = m_params.baseSpeed;
                m_wasLineLost = false;
                return;
            }
        }

        // In pure PID mode or extended loss: Recovery logic
        uint32_t lostDuration = millis() - m_lineLostStartTime;
        if (lostDuration < LINE_LOST_TIMEOUT_MS) {
            m_readings.status = LINE_RECOVERY;
            if (m_lastKnownPosition < 0) {
                outLeftSpeed = -(m_params.baseSpeed / 2);
                outRightSpeed = m_params.baseSpeed;
            } else {
                outLeftSpeed = m_params.baseSpeed;
                outRightSpeed = -(m_params.baseSpeed / 2);
            }
        } else {
            m_readings.status = LINE_LOST_STOPPED;
            outLeftSpeed = 0;
            outRightSpeed = 0;
        }
        return;
    }

    // Line is actively detected
    m_wasLineLost = false;

    // ========================================================================
    //  Case 3: 90-DEGREE TURNS & T-JUNCTIONS (Maze Navigation)
    // ========================================================================
    if (m_params.navMode != NAV_MODE_PID_LINE) {
        // Left Branch Detected (Left + Center on black, Right on white)
        if (normL > 650 && normC > 500 && normR < 350) {
            // Inch forward slightly to align robot wheel axis with intersection
            setMotorSpeed(m_params.baseSpeed, m_params.baseSpeed);
            delay(JUNCTION_INCH_TIME_MS);
            notifyMotorSafetyFeed();

            // Re-check center to see if line continues straight
            uint16_t afterC = normalizeSensor(analogRead(SENSOR_CENTER_PIN), m_params.minC, m_params.maxC, invert);
            bool canStraight = (afterC > 450);

            TurnAction action = decideTurnAtJunction(true, canStraight, false);
            executeTurn(action);
            outLeftSpeed = m_params.baseSpeed;
            outRightSpeed = m_params.baseSpeed;
            return;
        }

        // Right Branch Detected (Right + Center on black, Left on white)
        if (normR > 650 && normC > 500 && normL < 350) {
            // Inch forward slightly to align robot wheel axis with intersection
            setMotorSpeed(m_params.baseSpeed, m_params.baseSpeed);
            delay(JUNCTION_INCH_TIME_MS);
            notifyMotorSafetyFeed();

            // Re-check center to see if line continues straight
            uint16_t afterC = normalizeSensor(analogRead(SENSOR_CENTER_PIN), m_params.minC, m_params.maxC, invert);
            bool canStraight = (afterC > 450);

            TurnAction action = decideTurnAtJunction(false, canStraight, true);
            executeTurn(action);
            outLeftSpeed = m_params.baseSpeed;
            outRightSpeed = m_params.baseSpeed;
            return;
        }
    }

    // ========================================================================
    //  Case 4: NORMAL PID LINE TRACKING
    // ========================================================================
    // Compute Weighted Position Error:
    // Position = (-1000 * L + 0 * C + 1000 * R) / (L + C + R)
    int32_t weightedSum = (-1000LL * (int32_t)normL) + (1000LL * (int32_t)normR);
    int16_t position = (int16_t)(weightedSum / (int32_t)sum);
    m_readings.position = position;
    m_lastKnownPosition = position;
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
        case LINE_ON_TRACK:       return "Tracking";
        case LINE_INTERSECTION:   return "Intersection";
        case LINE_SHARP_LEFT:     return "Left Turn";
        case LINE_SHARP_RIGHT:    return "Right Turn";
        case LINE_DEAD_END:       return "Dead End";
        case LINE_CHECKPOINT:     return "Checkpoint";
        case LINE_END_ZONE:       return "Zone B Finish";
        case LINE_TURNING:        return "Turning";
        case LINE_RECOVERY:       return "Recovery";
        case LINE_LOST_STOPPED:   return "Stopped";
        case LINE_FINISHED:       return "Complete";
        default:                  return "Unknown";
    }
}
