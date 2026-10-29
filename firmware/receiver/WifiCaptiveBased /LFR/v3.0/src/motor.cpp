#include "motor.h"
#include <esp_arduino_version.h>

static uint32_t s_lastFeedTime = 0;
static bool s_isSafetyHalted = false;
static bool s_isTestMode = false;
static uint32_t s_testStartTime = 0;

static void writePwmLeft(uint8_t duty) {
#if defined(ESP_ARDUINO_VERSION) && ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    ledcWrite(ENA_PIN, duty);
#else
    ledcWrite(MOTOR_PWM_CHANNEL_LEFT, duty);
#endif
}

static void writePwmRight(uint8_t duty) {
#if defined(ESP_ARDUINO_VERSION) && ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    ledcWrite(ENB_PIN, duty);
#else
    ledcWrite(MOTOR_PWM_CHANNEL_RIGHT, duty);
#endif
}

void initMotors() {
    pinMode(IN1_PIN, OUTPUT);
    pinMode(IN2_PIN, OUTPUT);
    pinMode(IN3_PIN, OUTPUT);
    pinMode(IN4_PIN, OUTPUT);
    pinMode(STBY_PIN, OUTPUT);

    digitalWrite(IN1_PIN, LOW);
    digitalWrite(IN2_PIN, LOW);
    digitalWrite(IN3_PIN, LOW);
    digitalWrite(IN4_PIN, LOW);
    digitalWrite(STBY_PIN, HIGH); // Enable TB6612 H-bridge outputs

#if defined(ESP_ARDUINO_VERSION) && ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    ledcAttach(ENA_PIN, MOTOR_PWM_FREQ, MOTOR_PWM_RES_BITS);
    ledcAttach(ENB_PIN, MOTOR_PWM_FREQ, MOTOR_PWM_RES_BITS);
#else
    ledcSetup(MOTOR_PWM_CHANNEL_LEFT, MOTOR_PWM_FREQ, MOTOR_PWM_RES_BITS);
    ledcAttachPin(ENA_PIN, MOTOR_PWM_CHANNEL_LEFT);
    ledcSetup(MOTOR_PWM_CHANNEL_RIGHT, MOTOR_PWM_FREQ, MOTOR_PWM_RES_BITS);
    ledcAttachPin(ENB_PIN, MOTOR_PWM_CHANNEL_RIGHT);
#endif

    writePwmLeft(0);
    writePwmRight(0);
    s_lastFeedTime = millis();
    s_isSafetyHalted = false;
    s_isTestMode = false;

    Serial.println("[MOTOR] TB6612FNG Motors initialized (5kHz, 8-bit PWM).");
}

void setMotorStandby(bool standby) {
    if (standby) {
        emergencyStop();
        digitalWrite(STBY_PIN, LOW);
    } else {
        digitalWrite(STBY_PIN, HIGH);
    }
}

#include "calibration.h"

void setMotorSpeed(int16_t leftSpeed, int16_t rightSpeed) {
    digitalWrite(STBY_PIN, HIGH);
    s_isTestMode = false;

    // Handle motor channel swap if configured
    if (g_params.swapMotors) {
        int16_t tmp = leftSpeed;
        leftSpeed = rightSpeed;
        rightSpeed = tmp;
    }

    // Handle motor direction inversion
    if (g_params.invertLeftMotor) {
        leftSpeed = -leftSpeed;
    }
    if (g_params.invertRightMotor) {
        rightSpeed = -rightSpeed;
    }

    // Left motor (Motor A) direction & PWM
    if (leftSpeed > 0) {
        digitalWrite(IN1_PIN, HIGH);
        digitalWrite(IN2_PIN, LOW);
        uint8_t duty = (uint8_t)constrain(leftSpeed, 0, 255);
        writePwmLeft(duty);
    } else if (leftSpeed < 0) {
        digitalWrite(IN1_PIN, LOW);
        digitalWrite(IN2_PIN, HIGH);
        uint8_t duty = (uint8_t)constrain(-leftSpeed, 0, 255);
        writePwmLeft(duty);
    } else {
        digitalWrite(IN1_PIN, LOW);
        digitalWrite(IN2_PIN, LOW);
        writePwmLeft(0);
    }

    // Right motor (Motor B) direction & PWM
    if (rightSpeed > 0) {
        digitalWrite(IN3_PIN, HIGH);
        digitalWrite(IN4_PIN, LOW);
        uint8_t duty = (uint8_t)constrain(rightSpeed, 0, 255);
        writePwmRight(duty);
    } else if (rightSpeed < 0) {
        digitalWrite(IN3_PIN, LOW);
        digitalWrite(IN4_PIN, HIGH);
        uint8_t duty = (uint8_t)constrain(-rightSpeed, 0, 255);
        writePwmRight(duty);
    } else {
        digitalWrite(IN3_PIN, LOW);
        digitalWrite(IN4_PIN, LOW);
        writePwmRight(0);
    }
}

void emergencyStop() {
    digitalWrite(IN1_PIN, LOW);
    digitalWrite(IN2_PIN, LOW);
    digitalWrite(IN3_PIN, LOW);
    digitalWrite(IN4_PIN, LOW);
    writePwmLeft(0);
    writePwmRight(0);
    s_isTestMode = false;
}

void testMotor(const String& action, int16_t speed) {
    speed = constrain(speed, 0, 255);
    Serial.printf("[MOTOR TEST] Action='%s', Speed=%d\n", action.c_str(), speed);

    if (action == "stop" || speed == 0) {
        emergencyStop();
        return;
    }

    // Ensure TB6612 STBY is HIGH and activate test mode watchdog
    digitalWrite(STBY_PIN, HIGH);
    s_isTestMode = true;
    s_testStartTime = millis();
    s_isSafetyHalted = false;

    if (action == "lf") {
        setMotorSpeed(speed, 0);
    } else if (action == "lr") {
        setMotorSpeed(-speed, 0);
    } else if (action == "rf") {
        setMotorSpeed(0, speed);
    } else if (action == "rr") {
        setMotorSpeed(0, -speed);
    } else if (action == "forward" || action == "fwd") {
        setMotorSpeed(speed, speed);
    } else if (action == "reverse" || action == "rev") {
        setMotorSpeed(-speed, -speed);
    } else {
        emergencyStop();
    }
    // Re-assert test mode because setMotorSpeed clears it
    s_isTestMode = true;
}

void notifyMotorSafetyFeed() {
    s_lastFeedTime = millis();
    s_isSafetyHalted = false;
}

bool checkMotorSafety() {
    uint32_t now = millis();

    // In interactive bench test mode, allow up to 10 seconds per test command without requiring sensor polling
    if (s_isTestMode) {
        if (now - s_testStartTime > 10000) {
            Serial.println("[MOTOR TEST] Auto-stopped bench motor test after 10s safety timeout.");
            emergencyStop();
            return false;
        }
        return true;
    }

    // In running mode, require sensor updates at least every 500ms
    if (now - s_lastFeedTime > MOTOR_SAFETY_TIMEOUT_MS) {
        if (!s_isSafetyHalted) {
            Serial.println("[MOTOR SAFETY] Timeout! No sensor updates > 500ms. Halting motors.");
            emergencyStop();
            s_isSafetyHalted = true;
        }
        return false;
    }
    return true;
}
