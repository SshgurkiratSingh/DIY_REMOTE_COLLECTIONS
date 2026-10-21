#include "motor.h"
#include <esp_arduino_version.h>

static uint32_t s_lastFeedTime = 0;
static bool s_isSafetyHalted = false;

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

    digitalWrite(IN1_PIN, LOW);
    digitalWrite(IN2_PIN, LOW);
    digitalWrite(IN3_PIN, LOW);
    digitalWrite(IN4_PIN, LOW);

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

    Serial.println("[MOTOR] L298N Motors initialized (5kHz, 8-bit PWM).");
}

void setMotorSpeed(int16_t leftSpeed, int16_t rightSpeed) {
    // Left motor direction & PWM
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

    // Right motor direction & PWM
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
}

void testMotor(const String& action, int16_t speed) {
    speed = constrain(speed, 0, 255);
    Serial.printf("[MOTOR TEST] Action='%s', Speed=%d\n", action.c_str(), speed);

    if (action == "lf") {
        // Left Forward
        digitalWrite(IN1_PIN, HIGH);
        digitalWrite(IN2_PIN, LOW);
        writePwmLeft((uint8_t)speed);
        // Right Stop
        digitalWrite(IN3_PIN, LOW);
        digitalWrite(IN4_PIN, LOW);
        writePwmRight(0);
    } else if (action == "lr") {
        // Left Reverse
        digitalWrite(IN1_PIN, LOW);
        digitalWrite(IN2_PIN, HIGH);
        writePwmLeft((uint8_t)speed);
        // Right Stop
        digitalWrite(IN3_PIN, LOW);
        digitalWrite(IN4_PIN, LOW);
        writePwmRight(0);
    } else if (action == "rf") {
        // Right Forward
        digitalWrite(IN3_PIN, HIGH);
        digitalWrite(IN4_PIN, LOW);
        writePwmRight((uint8_t)speed);
        // Left Stop
        digitalWrite(IN1_PIN, LOW);
        digitalWrite(IN2_PIN, LOW);
        writePwmLeft(0);
    } else if (action == "rr") {
        // Right Reverse
        digitalWrite(IN3_PIN, LOW);
        digitalWrite(IN4_PIN, HIGH);
        writePwmRight((uint8_t)speed);
        // Left Stop
        digitalWrite(IN1_PIN, LOW);
        digitalWrite(IN2_PIN, LOW);
        writePwmLeft(0);
    } else if (action == "forward") {
        // Both Forward
        digitalWrite(IN1_PIN, HIGH);
        digitalWrite(IN2_PIN, LOW);
        writePwmLeft((uint8_t)speed);
        digitalWrite(IN3_PIN, HIGH);
        digitalWrite(IN4_PIN, LOW);
        writePwmRight((uint8_t)speed);
    } else if (action == "reverse") {
        // Both Reverse
        digitalWrite(IN1_PIN, LOW);
        digitalWrite(IN2_PIN, HIGH);
        writePwmLeft((uint8_t)speed);
        digitalWrite(IN3_PIN, LOW);
        digitalWrite(IN4_PIN, HIGH);
        writePwmRight((uint8_t)speed);
    } else {
        // Stop
        emergencyStop();
    }
}

void notifyMotorSafetyFeed() {
    s_lastFeedTime = millis();
    s_isSafetyHalted = false;
}

bool checkMotorSafety() {
    if (millis() - s_lastFeedTime > MOTOR_SAFETY_TIMEOUT_MS) {
        if (!s_isSafetyHalted) {
            Serial.println("[MOTOR SAFETY] Timeout! No sensor updates > 500ms. Halting motors.");
            emergencyStop();
            s_isSafetyHalted = true;
        }
        return false;
    }
    return true;
}
