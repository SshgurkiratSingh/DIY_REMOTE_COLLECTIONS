#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include <Arduino.h>

class MotorController
{
public:
  MotorController(int l_en, int l_in1, int l_in2,
                  int r_en, int r_in1, int r_in2);
  void begin();
  void driveLeft(int speed);
  void driveRight(int speed);
  void hillBrake();
  void stop();

private:
  int l_en_pin, l_in1_pin, l_in2_pin;
  int r_en_pin, r_in1_pin, r_in2_pin;

  void driveSingleMotor(int speed, int en, int in1, int in2);
};

#endif // MOTOR_CONTROLLER_H
