#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include <Arduino.h>

class MotorController {
public:
  MotorController(int l_rpwm, int l_lpwm, int l_len, int l_ren,
                  int r_rpwm, int r_lpwm, int r_len, int r_ren);
  void begin();
  void driveLeft(int speed);
  void driveRight(int speed);
  void hillBrake();
  void stop();

private:
  int l_rpwm_pin, l_lpwm_pin, l_len_pin, l_ren_pin;
  int r_rpwm_pin, r_lpwm_pin, r_len_pin, r_ren_pin;

  void driveSingleMotor(int speed, int rpwm, int lpwm, int len, int ren);
};

#endif // MOTOR_CONTROLLER_H
