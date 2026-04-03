#ifndef IMU_HANDLER_H
#define IMU_HANDLER_H

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

class IMUHandler
{
public:
    IMUHandler();
    bool begin();
    void update();

    float getYaw();
    float getPitch();
    float getRoll();
    void resetYaw();

private:
    Adafruit_MPU6050 mpu;
    float yaw;
    float pitch;
    float roll;

    float gyroZ_offset;

    unsigned long lastTime;

    void calibrateGyro();
};

#endif // IMU_HANDLER_H
