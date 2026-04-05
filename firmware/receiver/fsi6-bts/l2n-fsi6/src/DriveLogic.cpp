#include "DriveLogic.h"

DriveLogic::DriveLogic(MotorController *mCtrl)
{
    motors = mCtrl;
    targetYaw = 0;
    homeYaw = 0;
    isHeadingHoldActive = false;
}

void DriveLogic::update(ReceiverData input)
{
    if (input.failsafe)
    {
        motors->stop();
        return;
    }

    // Handle Mode 1 (100) = Direct Tank
    // Handle Mode 2 (0)   = Absolute Headless
    // Handle Mode 3 (-100)= Full Assist

    if (input.mode > 50)
    { // Mode 1
        applyTankDrive(input.ch1, input.ch2);
        isHeadingHoldActive = false;
    }
    else if (input.mode > -50 && input.mode < 50)
    { // Mode 2
        applyHeadlessDrive(input.ch1, input.ch2);
        isHeadingHoldActive = false;
    }
    else
    { // Mode 3
        applyFullAssistDrive(input.ch1, input.ch2);
    }
}

void DriveLogic::applyTankDrive(int ch1, int ch2)
{
    // Direct Mapping (Tank Drive) - Ch2 controls Left, Ch1 controls Right.
    motors->driveLeft(ch2);
    motors->driveRight(ch1);
}

void DriveLogic::applyHeadlessDrive(int x_input, int y_input)
{ // x=ch1, y=ch2
    // Fallback standard arcade drive
    int leftMotor = y_input + x_input;
    int rightMotor = y_input - x_input;

    motors->driveLeft(leftMotor);
    motors->driveRight(rightMotor);
}

void DriveLogic::applyFullAssistDrive(int ch1, int ch2)
{
    // Fallback standard arcade drive
    int leftMotor = ch2 + ch1;
    int rightMotor = ch2 - ch1;
    motors->driveLeft(leftMotor);
    motors->driveRight(rightMotor);
}

float DriveLogic::normalizeAngle(float angle)
{
    while (angle <= -180)
        angle += 360;
    while (angle > 180)
        angle -= 360;
    return angle;
}
