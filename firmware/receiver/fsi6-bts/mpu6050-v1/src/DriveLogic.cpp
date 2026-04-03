#include "DriveLogic.h"

DriveLogic::DriveLogic(MotorController *mCtrl, IMUHandler *iHandler)
{
    motors = mCtrl;
    imu = iHandler;
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

    if (input.setHome)
    {
        homeYaw = imu->getYaw();
        Serial.print("Home Yaw Set to: ");
        Serial.println(homeYaw);
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
    // We calculate angle and magnitude of joysticks
    float magnitude = sqrt(x_input * x_input + y_input * y_input);
    if (magnitude > 255.0)
        magnitude = 255.0;

    float stickAngle = atan2(x_input, y_input) * 180.0 / PI; // relative to forward

    // Vehicle current yaw difference to home
    float currentYaw = imu->getYaw();
    float diffAngle = normalizeAngle(currentYaw - homeYaw);

    // We rotate our stick input by diff angle (virtually) so forward remains Home's forward
    float translatedAngle = stickAngle - diffAngle;
    translatedAngle *= PI / 180.0;

    // Calculate raw translated inputs
    int transX = magnitude * sin(translatedAngle);
    int transY = magnitude * cos(translatedAngle);

    // Standard arcade mix using translated vectors
    int leftMotor = transY + transX;
    int rightMotor = transY - transX;

    motors->driveLeft(leftMotor);
    motors->driveRight(rightMotor);
}

void DriveLogic::applyFullAssistDrive(int ch1, int ch2)
{
    float pitch = imu->getPitch();
    float roll = imu->getRoll();

    // Anti-tip: If pitch >= 30, limit speed
    if (abs(pitch) > 30.0 || abs(roll) > 30.0)
    {
        if (ch2 > 100)
            ch2 = 100;
        if (ch2 < -100)
            ch2 = -100;
    }

    // Hill hold active brake
    if (ch1 == 0 && ch2 == 0)
    {
        if (abs(pitch) > 5.0)
        {
            motors->hillBrake();
            isHeadingHoldActive = false;
            return;
        }
    }

    // Heading hold logic
    float currentYaw = imu->getYaw();
    if (ch1 == 0)
    {
        // Driving straight
        if (!isHeadingHoldActive)
        {
            isHeadingHoldActive = true;
            targetYaw = currentYaw;
        }

        float yawError = normalizeAngle(targetYaw - currentYaw);
        // Simple P Controller
        float Kp = 2.0;
        int correction = yawError * Kp;

        // Add traction control limit
        if (correction > 150)
            correction = 150;
        if (correction < -150)
            correction = -150;
        ch1 += correction;
    }
    else
    {
        // User is steering, reset heading hold
        isHeadingHoldActive = false;
    }

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
