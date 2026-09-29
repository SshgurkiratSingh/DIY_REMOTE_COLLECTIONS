#include "MotorDriver.h"

MotorDriver::MotorDriver() : 
    targetSpeedA(0), currentSpeedA(0), 
    targetSpeedB(0), currentSpeedB(0),
    targetFwd(0), targetTurn(0),
    lastUpdateMs(0) {}

void MotorDriver::begin() {
    pinMode(IN1_PIN, OUTPUT);
    pinMode(IN2_PIN, OUTPUT);
    pinMode(IN3_PIN, OUTPUT);
    pinMode(IN4_PIN, OUTPUT);
    
    ledcSetup(PWM_CHANNEL_A, PWM_FREQ, PWM_RES_BITS);
    ledcSetup(PWM_CHANNEL_B, PWM_FREQ, PWM_RES_BITS);
    
    ledcAttachPin(ENA_PIN, PWM_CHANNEL_A);
    ledcAttachPin(ENB_PIN, PWM_CHANNEL_B);
    
    stop();
}

void MotorDriver::setMotorA(int speed) {
    speed = constrain(speed, -255, 255);
    if (speed > 0) { 
        digitalWrite(IN1_PIN, HIGH); 
        digitalWrite(IN2_PIN, LOW);  
        ledcWrite(PWM_CHANNEL_A, speed);  
    } else if (speed < 0) { 
        digitalWrite(IN1_PIN, LOW);  
        digitalWrite(IN2_PIN, HIGH); 
        ledcWrite(PWM_CHANNEL_A, -speed); 
    } else { 
        digitalWrite(IN1_PIN, LOW);  
        digitalWrite(IN2_PIN, LOW);  
        ledcWrite(PWM_CHANNEL_A, 0);      
    }
}

void MotorDriver::setMotorB(int speed) {
    speed = constrain(speed, -255, 255);
    if (speed > 0) { 
        digitalWrite(IN3_PIN, HIGH); 
        digitalWrite(IN4_PIN, LOW);  
        ledcWrite(PWM_CHANNEL_B, speed);  
    } else if (speed < 0) { 
        digitalWrite(IN3_PIN, LOW);  
        digitalWrite(IN4_PIN, HIGH); 
        ledcWrite(PWM_CHANNEL_B, -speed); 
    } else { 
        digitalWrite(IN3_PIN, LOW);  
        digitalWrite(IN4_PIN, LOW);  
        ledcWrite(PWM_CHANNEL_B, 0);      
    }
}

void MotorDriver::stop() {
    setMotorA(0);
    setMotorB(0);
    currentSpeedA = targetSpeedA = 0;
    currentSpeedB = targetSpeedB = 0;
    targetFwd = targetTurn = 0;
}

int MotorDriver::rampTowards(int current, int target, int step) {
    if (current < target) return min(current + step, target);
    if (current > target) return max(current - step, target);
    return current;
}

void MotorDriver::mixDrive(uint16_t throttleRaw, uint16_t steerRaw, uint16_t potRaw) {
    int tRaw = (int)throttleRaw;
    int sRaw = (int)steerRaw;
    
    // Apply deadzone to raw ADC values around center
    if (abs(tRaw - JOY_CENTER) < JOY_DEADZONE_RAW) tRaw = JOY_CENTER;
    if (abs(sRaw - JOY_CENTER) < JOY_DEADZONE_RAW) sRaw = JOY_CENTER;

    targetFwd  = map(tRaw, 0, JOY_MAX, -255, 255);
    targetTurn = map(sRaw, 0, JOY_MAX, -255, 255);

    // Apply potentiometer as a global speed limiter (0 - 255)
    int maxSpeed = map(potRaw, 0, 4095, 0, 255);
    
    targetFwd = (targetFwd * maxSpeed) / 255;
    targetTurn = (targetTurn * maxSpeed) / 255;

    targetSpeedA = constrain(targetFwd + targetTurn, -255, 255);
    targetSpeedB = constrain(targetFwd - targetTurn, -255, 255);
}

void MotorDriver::update() {
    unsigned long now = millis();
    // Update at ~50Hz (every 20ms) for smooth ramping
    if (now - lastUpdateMs >= 20) {
        lastUpdateMs = now;
        currentSpeedA = rampTowards(currentSpeedA, targetSpeedA, RAMP_STEP);
        currentSpeedB = rampTowards(currentSpeedB, targetSpeedB, RAMP_STEP);
        setMotorA(currentSpeedA);
        setMotorB(currentSpeedB);
    }
}
