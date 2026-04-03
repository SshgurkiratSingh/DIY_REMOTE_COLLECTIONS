#include "DriveModes.h"

namespace
{
    const int PWM_MAX = 255;
    const int JOY_MAX = 4095;
    const int JOY_DEADZONE = 120;
    const int BURST_SPEED_LIMIT = 255;

    const float MIN_RAMP_RATE = 100.0f;
    const float MAX_RAMP_RATE = 1000.0f;

    const float PI_F = 3.14159265f;
    const float ANTI_TIP_LIMIT_DEG = 30.0f;
    const float HILL_HOLD_PITCH_DEG = 5.0f;
    const float HILL_HOLD_GAIN = 2.2f;
    const float HEADING_HOLD_GAIN = 140.0f;
}

DriveModes::DriveModes()
    : prevPush2_(false),
      homeYawRad_(0.0f),
      headingHoldYawRad_(0.0f),
      headingHoldArmed_(false),
      tractionScale_(1.0f)
{
}

float DriveModes::normalizeAngle(float angleRad) const
{
    while (angleRad > PI_F)
    {
        angleRad -= 2.0f * PI_F;
    }
    while (angleRad < -PI_F)
    {
        angleRad += 2.0f * PI_F;
    }
    return angleRad;
}

void DriveModes::updateHomeDirectionOnPush2Rising(bool push2Pressed, float currentYawRad)
{
    if (push2Pressed && !prevPush2_)
    {
        homeYawRad_ = currentYawRad;
    }
    prevPush2_ = push2Pressed;
}

int DriveModes::mapJoystickSigned(uint16_t raw) const
{
    const int mapped = map(raw, 0, JOY_MAX, -PWM_MAX, PWM_MAX);
    if (abs(mapped) < JOY_DEADZONE)
    {
        return 0;
    }
    return mapped;
}

int DriveModes::speedLimitFromPot(uint16_t potValue) const
{
    return map(potValue, 0, JOY_MAX, 0, PWM_MAX);
}

DriveOutput DriveModes::compute(const struct_message &msg, const ImuState &imu, float dt)
{
    const bool burstActive = msg.push1;
    const bool mode2Headless = msg.toggle2;
    const bool mode3FullAssist = msg.toggle1;

    const int baseSpeedLimit = burstActive ? BURST_SPEED_LIMIT : speedLimitFromPot(msg.potValue);
    const float rampRate = map((long)msg.potValue, 0, JOY_MAX, (long)MIN_RAMP_RATE, (long)MAX_RAMP_RATE);

    int steer = mapJoystickSigned(msg.joyX);
    int throttle = mapJoystickSigned(msg.joyY);

    if (mode2Headless)
    {
        const float offset = normalizeAngle(imu.yawRad - homeYawRad_);
        const float c = cosf(offset);
        const float s = sinf(offset);

        const float worldX = static_cast<float>(steer);
        const float worldY = static_cast<float>(throttle);

        const float localX = (worldX * c) + (worldY * s);
        const float localY = (-worldX * s) + (worldY * c);

        steer = static_cast<int>(localX);
        throttle = static_cast<int>(localY);
    }

    int speedLimit = baseSpeedLimit;

    if (mode3FullAssist)
    {
        if (abs(imu.pitchDeg) > ANTI_TIP_LIMIT_DEG || abs(imu.rollDeg) > ANTI_TIP_LIMIT_DEG)
        {
            throttle = 0;
            steer = 0;
        }

        const bool steerNeutral = abs(steer) < JOY_DEADZONE;
        if (steerNeutral)
        {
            if (!headingHoldArmed_)
            {
                headingHoldYawRad_ = imu.yawRad;
                headingHoldArmed_ = true;
            }

            const float yawErr = normalizeAngle(headingHoldYawRad_ - imu.yawRad);
            steer += static_cast<int>(yawErr * HEADING_HOLD_GAIN);
        }
        else
        {
            headingHoldArmed_ = false;
            headingHoldYawRad_ = imu.yawRad;
        }

        const bool throttleNeutral = abs(throttle) < JOY_DEADZONE;
        if (throttleNeutral && abs(imu.pitchDeg) > HILL_HOLD_PITCH_DEG)
        {
            const float hold = -imu.pitchDeg * HILL_HOLD_GAIN;
            throttle = constrain(static_cast<int>(hold), -PWM_MAX / 3, PWM_MAX / 3);
        }

        const bool highCommand = abs(throttle) > (baseSpeedLimit * 0.65f);
        const bool lowResponse = abs(imu.forwardAccel) < 0.7f;
        if (highCommand && lowResponse)
        {
            tractionScale_ -= dt * 1.4f;
        }
        else
        {
            tractionScale_ += dt * 0.6f;
        }
        tractionScale_ = constrain(tractionScale_, 0.45f, 1.0f);

        speedLimit = static_cast<int>(baseSpeedLimit * tractionScale_);
    }
    else
    {
        headingHoldArmed_ = false;
        tractionScale_ = 1.0f;
    }

    int leftSpeed = throttle + steer;
    int rightSpeed = throttle - steer;

    leftSpeed = constrain(leftSpeed, -speedLimit, speedLimit);
    rightSpeed = constrain(rightSpeed, -speedLimit, speedLimit);

    DriveOutput out = {
        leftSpeed,
        rightSpeed,
        mode3FullAssist,
        burstActive,
        rampRate,
        mode2Headless,
        mode3FullAssist,
    };

    return out;
}
