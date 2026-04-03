#include "DriveLogic.h"

DriveLogic::DriveLogic(MotorController *mCtrl)
{
    motors = mCtrl;
}

void DriveLogic::update(ReceiverData input)
{
    if (input.failsafe)
    {
        motors->stop();
        return;
    }

    if (input.setHome)
    {
        // Custom action for Ch6 button press could be added here
        Serial.println("Action Button (Ch6) Pressed");
    }

    // Handle Mode 1 (100) = Direct Tank
    // Handle Mode 2 (0)   = Standard Arcade
    // Handle Mode 3 (-100)= Dynamic Speed Arcade

    if (input.mode > 50)
    { // Mode 1
        applyTankDrive(input.ch1, input.ch2);
    }
    else if (input.mode > -50 && input.mode < 50)
    { // Mode 2
        applyArcadeDrive(input.ch1, input.ch2, input.speedScale, input.burstMode);
    }
    else
    { // Mode 3
        applyDynamicSpeedDrive(input.ch1, input.ch2, input.speedScale, input.burstMode);
    }
}

void DriveLogic::applyTankDrive(int ch1, int ch2)
{
    // Direct Mapping (Tank Drive) - Ch2 controls Left, Ch1 controls Right.
    motors->driveLeft(ch2);
    motors->driveRight(ch1);
}

void DriveLogic::applyArcadeDrive(int ch1, int ch2, float speedScale, bool burstMode)
{
    int leftMotor = ch2 + ch1;
    int rightMotor = ch2 - ch1;

    motors->driveLeft(leftMotor);
    motors->driveRight(rightMotor);
}

void DriveLogic::applyDynamicSpeedDrive(int ch1, int ch2, float speedScale, bool burstMode)
{
    // Dynamic Speed mapping - apply dual rates and burst functionality
    int leftMotor = ch2 + ch1;
    int rightMotor = ch2 - ch1;

    if (burstMode)
    {
        // Boost multiplier if burst mode is active
        leftMotor = constrain(leftMotor * 1.5, -255, 255);
        rightMotor = constrain(rightMotor * 1.5, -255, 255);
    }
    else
    {
        // Fine control mode
        leftMotor = leftMotor * 0.7;
        rightMotor = rightMotor * 0.7;
    }

    motors->driveLeft(leftMotor);
    motors->driveRight(rightMotor);
}
