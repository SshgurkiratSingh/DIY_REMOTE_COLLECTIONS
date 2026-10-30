#include "pid.h"
#include <math.h>
#include "motor.h"

PIDController g_pid;

// Sensor positional weights: Far Left (-2000) to Far Right (+2000)
static const int32_t SENSOR_WEIGHTS[NUM_SENSORS] = {
    -2000, -1000, 0, 1000, 2000
};

PIDController::PIDController()
    : m_lastError(0.0f),
      m_integral(0.0f),
      m_lastKnownPosition(0),
      m_lineLostStartTime(0),
      m_wasLineLost(false),
      m_lastCheckpointTime(0),
      m_allBlackStartTime(0),
      m_allBlackCandidate(false),
      m_pathLength(0),
      m_cycleTurnCount(0),
      m_lastTurnChar(0)
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

    m_cycleTurnCount = 0;
    m_lastTurnChar = 0;

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

void PIDController::updateSensorsOnly(const uint16_t raw[NUM_SENSORS]) {
    bool invert = (m_params.invertLine != 0);
    uint32_t sum = 0;
    int32_t weightedSum = 0;
    uint8_t countOnLine = 0;

    for (int i = 0; i < NUM_SENSORS; i++) {
        uint16_t norm = normalizeSensor(raw[i], m_params.minVal[i], m_params.maxVal[i], invert);
        m_readings.raw[i] = raw[i];
        m_readings.norm[i] = norm;

        if (!invert) {
            m_readings.onLine[i] = (raw[i] >= m_params.thresh[i]);
        } else {
            m_readings.onLine[i] = (raw[i] <= m_params.thresh[i]);
        }

        if (m_readings.onLine[i]) {
            countOnLine++;
        }

        uint16_t effectiveNorm = (norm >= LINE_NOISE_FLOOR) ? norm : 0;
        sum += effectiveNorm;
        weightedSum += (SENSOR_WEIGHTS[i] * (int32_t)effectiveNorm);
    }

    if (sum >= LINE_MIN_SUM) {
        m_readings.position = (int16_t)(weightedSum / (int32_t)sum);
        m_lastKnownPosition = m_readings.position;
        m_readings.status = LINE_ON_TRACK;
    } else {
        m_readings.position = m_lastKnownPosition;
        m_readings.status = LINE_LOST_STOPPED;
    }
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

    // Mode 3: Standard PID Line Follower - at non-branching elbow corners (canLeft != canRight),
    // automatically follow the bend! If multiple branches exist, default to straight, then left, then right.
    if (m_params.navMode == NAV_MODE_PID_LINE) {
        if (canStraight) return TURN_STRAIGHT;
        if (canLeft && !canRight) return TURN_LEFT_90;
        if (canRight && !canLeft) return TURN_RIGHT_90;
        if (canLeft) return TURN_LEFT_90;
        if (canRight) return TURN_RIGHT_90;
        return TURN_NONE;
    }

    // Mode 0: Autonomous Left-Hand Rule (LSRB) with Smart Loop-Breaking
    // If we have taken 3 or more 'L' turns in succession (potential loop cycle)
    // and an alternative branch (Straight or Right) is available:
    if (m_cycleTurnCount >= 3) {
        if (canStraight) {
            Serial.println("[LOOP BREAKER] Cycle detected! Suppressing 'L', forcing 'S' to escape loop.");
            m_cycleTurnCount = 0;
            return TURN_STRAIGHT;
        }
        if (canRight) {
            Serial.println("[LOOP BREAKER] Cycle detected! Suppressing 'L', forcing 'R' to escape loop.");
            m_cycleTurnCount = 0;
            return TURN_RIGHT_90;
        }
    }

    if (canLeft) {
        m_cycleTurnCount++;
        return TURN_LEFT_90;
    }
    if (canStraight) {
        m_cycleTurnCount = 0;
        return TURN_STRAIGHT;
    }
    if (canRight) {
        m_cycleTurnCount = 0;
        return TURN_RIGHT_90;
    }
    return TURN_UTURN_180;
}

void PIDController::executeTurn(TurnAction action) {
    if (action == TURN_NONE) return;

    bool recordTurn = (m_params.navMode != NAV_MODE_CUSTOM_PATH);

    if (action == TURN_STRAIGHT) {
        if (recordTurn) logTurn('S');
        // Inch straight through intersection to clear transverse line
        setMotorSpeed(m_params.baseSpeed, m_params.baseSpeed);
        delay(JUNCTION_INCH_TIME_MS);
        notifyMotorSafetyFeed();
        return;
    }

    m_readings.status = LINE_TURNING;
    int16_t turnSpd = TURN_SPEED;
    uint8_t centerPin = (m_params.sensorPins[SENSOR_CENTER_INDEX] != 0) ? m_params.sensorPins[SENSOR_CENTER_INDEX] : SENSOR_CENTER_PIN;
    bool invert = (m_params.invertLine != 0);

    if (action == TURN_LEFT_90) {
        if (recordTurn) logTurn('L');
        Serial.println("[NAV] Executing LEFT turn...");
        // Pivot left: Left motor reverse, Right motor forward
        setMotorSpeed(-turnSpd, turnSpd);

        // 1. Brief impulse (35ms) to begin rotation
        uint32_t start = millis();
        while (millis() - start < 35) {
            notifyMotorSafetyFeed();
            delay(5);
        }

        // 2. Wait until center sensor leaves old line
        while (millis() - start < 250) {
            uint16_t cNorm = normalizeSensor(readSensorRaw(centerPin), m_params.minVal[SENSOR_CENTER_INDEX], m_params.maxVal[SENSOR_CENTER_INDEX], invert);
            if (cNorm < 300) break;
            notifyMotorSafetyFeed();
            delay(5);
        }

        // 3. Actively acquire new line (locks cleanly onto 30-deg, 45-deg, 90-deg branches)
        bool acquired = false;
        while (millis() - start < TURN_TIMEOUT_MS) {
            uint16_t cNorm = normalizeSensor(readSensorRaw(centerPin), m_params.minVal[SENSOR_CENTER_INDEX], m_params.maxVal[SENSOR_CENTER_INDEX], invert);
            if (cNorm > 450) {
                acquired = true;
                break;
            }
            notifyMotorSafetyFeed();
            delay(4);
        }

        // Active counter-brake to kill rotational inertia
        setMotorSpeed(turnSpd / 2, -turnSpd / 2);
        delay(25);
        setMotorSpeed(0, 0);

        if (!acquired) {
            Serial.println("[NAV] Left turn timed out without line re-acquisition.");
        }
    } else if (action == TURN_RIGHT_90) {
        if (recordTurn) logTurn('R');
        Serial.println("[NAV] Executing RIGHT turn...");
        // Pivot right: Left motor forward, Right motor reverse
        setMotorSpeed(turnSpd, -turnSpd);

        // 1. Brief impulse (35ms) to begin rotation
        uint32_t start = millis();
        while (millis() - start < 35) {
            notifyMotorSafetyFeed();
            delay(5);
        }

        // 2. Wait until center sensor leaves old line
        while (millis() - start < 250) {
            uint16_t cNorm = normalizeSensor(readSensorRaw(centerPin), m_params.minVal[SENSOR_CENTER_INDEX], m_params.maxVal[SENSOR_CENTER_INDEX], invert);
            if (cNorm < 300) break;
            notifyMotorSafetyFeed();
            delay(5);
        }

        // 3. Actively acquire new line
        bool acquired = false;
        while (millis() - start < TURN_TIMEOUT_MS) {
            uint16_t cNorm = normalizeSensor(readSensorRaw(centerPin), m_params.minVal[SENSOR_CENTER_INDEX], m_params.maxVal[SENSOR_CENTER_INDEX], invert);
            if (cNorm > 450) {
                acquired = true;
                break;
            }
            notifyMotorSafetyFeed();
            delay(4);
        }

        // Active counter-brake
        setMotorSpeed(-turnSpd / 2, turnSpd / 2);
        delay(25);
        setMotorSpeed(0, 0);

        if (!acquired) {
            Serial.println("[NAV] Right turn timed out without line re-acquisition.");
        }
    } else if (action == TURN_UTURN_180) {
        if (recordTurn) logTurn('B');
        m_cycleTurnCount = 0; // Dead end encountered, reset cycle counter
        Serial.println("[NAV] Dead end! Executing 180-deg U-TURN...");
        // Pivot around: Left motor reverse, Right motor forward
        setMotorSpeed(-turnSpd, turnSpd);

        // 1. Impulse
        uint32_t start = millis();
        while (millis() - start < 150) {
            notifyMotorSafetyFeed();
            delay(5);
        }

        // 2. Clear old line
        while (millis() - start < 400) {
            uint16_t cNorm = normalizeSensor(readSensorRaw(centerPin), m_params.minVal[SENSOR_CENTER_INDEX], m_params.maxVal[SENSOR_CENTER_INDEX], invert);
            if (cNorm < 300) break;
            notifyMotorSafetyFeed();
            delay(5);
        }

        // 3. Acquire 180-deg line
        while (millis() - start < TURN_TIMEOUT_MS) {
            uint16_t cNorm = normalizeSensor(readSensorRaw(centerPin), m_params.minVal[SENSOR_CENTER_INDEX], m_params.maxVal[SENSOR_CENTER_INDEX], invert);
            if (cNorm > 450) {
                break;
            }
            notifyMotorSafetyFeed();
            delay(4);
        }

        // Active counter-brake
        setMotorSpeed(turnSpd / 2, -turnSpd / 2);
        delay(30);
        setMotorSpeed(0, 0);
    }

    // Brief brake & reset integral
    delay(20);
    m_integral = 0.0f;
    m_lastError = 0.0f;
    notifyMotorSafetyFeed();
}

void PIDController::compute(const uint16_t raw[NUM_SENSORS],
                            int16_t& outLeftSpeed, int16_t& outRightSpeed) {
    if (m_readings.runFinished) {
        outLeftSpeed = 0;
        outRightSpeed = 0;
        return;
    }

    bool invert = (m_params.invertLine != 0);

    // 1. Normalize all 5 sensor values to 0 - 1000 & calculate onLine state
    uint32_t sum = 0;
    int32_t weightedSum = 0;
    uint8_t countOnLine = 0;

    uint8_t centerPin = (m_params.sensorPins[SENSOR_CENTER_INDEX] != 0) ? m_params.sensorPins[SENSOR_CENTER_INDEX] : SENSOR_CENTER_PIN;

    for (int i = 0; i < NUM_SENSORS; i++) {
        uint16_t norm = normalizeSensor(raw[i], m_params.minVal[i], m_params.maxVal[i], invert);
        m_readings.raw[i] = raw[i];
        m_readings.norm[i] = norm;

        if (!invert) {
            m_readings.onLine[i] = (raw[i] >= m_params.thresh[i]);
        } else {
            m_readings.onLine[i] = (raw[i] <= m_params.thresh[i]);
        }

        if (m_readings.onLine[i]) {
            countOnLine++;
        }

        uint16_t effectiveNorm = (norm >= LINE_NOISE_FLOOR) ? norm : 0;
        sum += effectiveNorm;
        weightedSum += (SENSOR_WEIGHTS[i] * (int32_t)effectiveNorm);
    }

    // ========================================================================
    //  Case 1: DEAD END OR LINE LOST (All sensors on white)
    // ========================================================================
    if (countOnLine == 0 && sum < LINE_MIN_SUM) {
        if (!m_wasLineLost) {
            m_wasLineLost = true;
            m_lineLostStartTime = millis();
        }

        // In Maze Solving modes: Dead end requires 180° U-turn!
        if (m_params.navMode != NAV_MODE_PID_LINE) {
            delay(50);
            // Re-read sensors to confirm true dead end
            uint32_t confirmSum = 0;
            for (int i = 0; i < NUM_SENSORS; i++) {
                uint8_t pin = (m_params.sensorPins[i] != 0) ? m_params.sensorPins[i] : SENSOR_PINS[i];
                uint16_t cVal = normalizeSensor(readSensorRaw(pin), m_params.minVal[i], m_params.maxVal[i], invert);
                confirmSum += (cVal >= LINE_NOISE_FLOOR) ? cVal : 0;
            }

            if (confirmSum < 300) {
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
            int16_t revSpeed = (m_params.minSpeed < 0) ? -(m_params.baseSpeed / 2) : m_params.minSpeed;
            if (m_lastKnownPosition < 0) {
                outLeftSpeed = revSpeed;
                outRightSpeed = m_params.baseSpeed;
            } else {
                outLeftSpeed = m_params.baseSpeed;
                outRightSpeed = revSpeed;
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
    //  Case 2: JUNCTIONS, TURNS, CROSSBARS & ZONE B CLASSIFIER
    // ========================================================================
    // Active line branches or transverse bars:
    bool leftDetected = (m_readings.norm[0] > 450 || (m_readings.norm[0] > 350 && m_readings.norm[1] > 380));
    bool rightDetected = (m_readings.norm[4] > 450 || (m_readings.norm[4] > 350 && m_readings.norm[3] > 380));
    bool crossDetected = (countOnLine >= 4 || sum > 3200);

    if (leftDetected || rightDetected || crossDetected) {
        // Inch forward by axle-to-sensor offset (approx 35mm) to place drive axle over junction center
        setMotorSpeed(m_params.baseSpeed, m_params.baseSpeed);
        delay(JUNCTION_INCH_TIME_MS);
        notifyMotorSafetyFeed();

        // Sample all sensors at junction center
        uint8_t postCount = 0;
        uint32_t postSum = 0;
        for (int i = 0; i < NUM_SENSORS; i++) {
            uint8_t pin = (m_params.sensorPins[i] != 0) ? m_params.sensorPins[i] : SENSOR_PINS[i];
            uint16_t n = normalizeSensor(readSensorRaw(pin), m_params.minVal[i], m_params.maxVal[i], invert);
            postSum += (n >= LINE_NOISE_FLOOR) ? n : 0;
            if (n >= 400) postCount++;
        }

        // --------------------------------------------------------------------
        //  1. Check for TRUE Zone B (Large Black Square >= 25cm x 25cm)
        // --------------------------------------------------------------------
        if (postCount >= 4 && postSum > 3400) {
            // Inch forward another 100ms: only a massive square remains fully black!
            setMotorSpeed(m_params.baseSpeed / 2, m_params.baseSpeed / 2);
            delay(100);
            notifyMotorSafetyFeed();

            uint8_t confirmCount = 0;
            for (int i = 0; i < NUM_SENSORS; i++) {
                uint8_t pin = (m_params.sensorPins[i] != 0) ? m_params.sensorPins[i] : SENSOR_PINS[i];
                uint16_t n = normalizeSensor(readSensorRaw(pin), m_params.minVal[i], m_params.maxVal[i], invert);
                if (n >= 400) confirmCount++;
            }

            if (confirmCount >= 4) {
                // Confirmed Zone B End Point!
                m_readings.status = LINE_END_ZONE;
                m_readings.runFinished = true;
                outLeftSpeed = 0;
                outRightSpeed = 0;
                emergencyStop();
                Serial.println("\n***************************************************");
                Serial.println("   [ZONE B REACHED] End Point Detected! RUN COMPLETE!   ");
                Serial.printf("   Checkpoints logged: %u / 4\n", m_readings.checkpoints);
                Serial.printf("   Shortest Solved Route: %s\n", m_readings.solvedPath);
                Serial.println("***************************************************\n");
                return;
            }
        }

        // --------------------------------------------------------------------
        //  2. Check for Junctions (T-Junction, 4-Way Cross, Branches, Checkpoints)
        // --------------------------------------------------------------------
        uint16_t cNorm = normalizeSensor(readSensorRaw(centerPin), m_params.minVal[SENSOR_CENTER_INDEX], m_params.maxVal[SENSOR_CENTER_INDEX], invert);
        bool canStraight = (cNorm > 400);
        bool canLeft = leftDetected || (crossDetected && !canStraight);
        bool canRight = rightDetected || (crossDetected && !canStraight);

        // Check if this was purely a Transverse Checkpoint Line (rulebook: 4 checkpoints)
        if (crossDetected && canStraight && !leftDetected && !rightDetected) {
            if (millis() - m_lastCheckpointTime > CHECKPOINT_DEBOUNCE_MS) {
                m_lastCheckpointTime = millis();
                m_readings.checkpoints++;
                m_readings.status = LINE_CHECKPOINT;
                Serial.printf("[CHECKPOINT] Passed checkpoint #%u (Transverse line)\n", m_readings.checkpoints);
            }
            outLeftSpeed = m_params.baseSpeed;
            outRightSpeed = m_params.baseSpeed;
            return;
        }

        // Branching / Turning Junction
        if (canLeft && canRight && !canStraight) {
            m_readings.status = LINE_INTERSECTION;
            Serial.println("[JUNCTION] T-Junction detected (Left + Right, no straight)!");
        } else if (canLeft && canRight && canStraight) {
            m_readings.status = LINE_INTERSECTION;
            Serial.println("[JUNCTION] 4-Way Cross (+) detected!");
        } else if (canLeft) {
            m_readings.status = LINE_SHARP_LEFT;
            Serial.println("[JUNCTION] Left branch / turn detected!");
        } else if (canRight) {
            m_readings.status = LINE_SHARP_RIGHT;
            Serial.println("[JUNCTION] Right branch / turn detected!");
        }

        TurnAction action = decideTurnAtJunction(canLeft, canStraight, canRight);
        executeTurn(action);
        outLeftSpeed = m_params.baseSpeed;
        outRightSpeed = m_params.baseSpeed;
        return;
    }

    // ========================================================================
    //  Case 4: NORMAL PID LINE TRACKING (5-Sensor Weighted Average)
    // ========================================================================
    // Position = Sum(Weight[i] * Norm[i]) / Sum(Norm[i])
    // Output range: -2000 to +2000
    int16_t position = 0;
    if (sum >= LINE_MIN_SUM) {
        position = (int16_t)(weightedSum / (int32_t)sum);
    } else {
        position = m_lastKnownPosition;
    }

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
