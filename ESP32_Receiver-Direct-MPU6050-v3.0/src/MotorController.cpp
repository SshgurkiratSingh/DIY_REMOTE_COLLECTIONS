#include "MotorController.h"

namespace
{
    const int PWM_FREQ = 5000;
    const int PWM_RESOLUTION = 8;
    const int PWM_MAX = 255;
}

MotorController::MotorController(const MotorDriver &leftMotor, const MotorDriver &rightMotor)
    : left_(leftMotor),
      right_(rightMotor),
      targetLeft_(0),
      targetRight_(0),
      currentLeft_(0.0f),
      currentRight_(0.0f),
      enableRamp_(false),
      burstActive_(false),
      rampRate_(1000.0f)
{
}

void MotorController::begin()
{
    initMotor(left_);
    initMotor(right_);
}

void MotorController::initMotor(const MotorDriver &motor)
{
    ledcSetup(motor.chanForward, PWM_FREQ, PWM_RESOLUTION);
    ledcSetup(motor.chanReverse, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(motor.rpwmPin, motor.chanForward);
    ledcAttachPin(motor.lpwmPin, motor.chanReverse);

    pinMode(motor.lenPin, OUTPUT);
    pinMode(motor.renPin, OUTPUT);
    digitalWrite(motor.lenPin, HIGH);
    digitalWrite(motor.renPin, HIGH);

    ledcWrite(motor.chanForward, 0);
    ledcWrite(motor.chanReverse, 0);
}

void MotorController::setMotor(const MotorDriver &motor, int speed)
{
    speed = constrain(speed, -PWM_MAX, PWM_MAX);

    if (speed >= 0)
    {
        ledcWrite(motor.chanForward, speed);
        ledcWrite(motor.chanReverse, 0);
    }
    else
    {
        ledcWrite(motor.chanForward, 0);
        ledcWrite(motor.chanReverse, -speed);
    }
}

void MotorController::setTargets(int left, int right, bool enableRamp, bool burstActive, float rampRate)
{
    targetLeft_ = constrain(left, -PWM_MAX, PWM_MAX);
    targetRight_ = constrain(right, -PWM_MAX, PWM_MAX);
    enableRamp_ = enableRamp;
    burstActive_ = burstActive;
    rampRate_ = rampRate;
}

void MotorController::update(float dt)
{
    if (targetLeft_ == 0 && targetRight_ == 0)
    {
        currentLeft_ = 0;
        currentRight_ = 0;
    }
    else if (enableRamp_ && !burstActive_)
    {
        const float step = rampRate_ * dt;

        if (currentLeft_ < targetLeft_)
        {
            currentLeft_ += step;
            if (currentLeft_ > targetLeft_)
            {
                currentLeft_ = targetLeft_;
            }
        }
        else if (currentLeft_ > targetLeft_)
        {
            currentLeft_ -= step;
            if (currentLeft_ < targetLeft_)
            {
                currentLeft_ = targetLeft_;
            }
        }

        if (currentRight_ < targetRight_)
        {
            currentRight_ += step;
            if (currentRight_ > targetRight_)
            {
                currentRight_ = targetRight_;
            }
        }
        else if (currentRight_ > targetRight_)
        {
            currentRight_ -= step;
            if (currentRight_ < targetRight_)
            {
                currentRight_ = targetRight_;
            }
        }
    }
    else
    {
        currentLeft_ = targetLeft_;
        currentRight_ = targetRight_;
    }

    setMotor(left_, static_cast<int>(currentLeft_));
    setMotor(right_, static_cast<int>(currentRight_));
}
