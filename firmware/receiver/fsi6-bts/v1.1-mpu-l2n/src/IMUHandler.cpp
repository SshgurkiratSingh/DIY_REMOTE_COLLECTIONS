#include "IMUHandler.h"

IMUHandler::IMUHandler()
{
    yaw = 0.0;
    pitch = 0.0;
    roll = 0.0;
    gyroZ_offset = 0.0;
    lastTime = 0;
}

bool IMUHandler::begin()
{
    if (!mpu.begin())
    {
        Serial.println("Failed to find MPU6050 chip");
        return false;
    }

    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    calibrateGyro();
    lastTime = millis();

    return true;
}

void IMUHandler::calibrateGyro()
{
    Serial.println("Calibrating Gyro (Keep still)...");
    long sumZ = 0;
    int samples = 500;

    for (int i = 0; i < samples; i++)
    {
        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);
        sumZ += g.gyro.z * 1000; // scale up to avoid float loss
        delay(3);
    }

    gyroZ_offset = (sumZ / (float)samples) / 1000.0;
    Serial.print("Gyro Z Offset: ");
    Serial.println(gyroZ_offset);
    yaw = 0;
}

void IMUHandler::update()
{
    unsigned long currentTime = millis();
    float dt = (currentTime - lastTime) / 1000.0;
    lastTime = currentTime;

    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    // High pass filter for gyro to discard drift (simple approach) or just integrate
    float gz = g.gyro.z - gyroZ_offset;

    // Ignore noise floor (deadband for gyro)
    if (abs(gz) < 0.03)
        gz = 0.0;

    // Integrate gyro to yaw (convert rad/s to deg/s, then multiply by dt)
    yaw += (gz * 57.2958) * dt;

    // Quick acc to pitch and roll (simplified)
    pitch = atan2(a.acceleration.x, sqrt(a.acceleration.y * a.acceleration.y + a.acceleration.z * a.acceleration.z)) * 180.0 / PI;
    roll = atan2(-a.acceleration.y, a.acceleration.z) * 180.0 / PI;
}

float IMUHandler::getYaw()
{
    return yaw;
}

float IMUHandler::getPitch()
{
    return pitch;
}

float IMUHandler::getRoll()
{
    return roll;
}

void IMUHandler::resetYaw()
{
    yaw = 0;
}
