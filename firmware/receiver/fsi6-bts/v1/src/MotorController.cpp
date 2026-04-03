#include "MotorController.h"

MotorController::MotorController(int l_rpwm, int l_lpwm, int l_len, int l_ren,
                                 int r_rpwm, int r_lpwm, int r_len, int r_ren)
{
    l_rpwm_pin = l_rpwm;
    l_lpwm_pin = l_lpwm;
    l_len_pin = l_len;
    l_ren_pin = l_ren;

    r_rpwm_pin = r_rpwm;
    r_lpwm_pin = r_lpwm;
    r_len_pin = r_len;
    r_ren_pin = r_ren;
}

void MotorController::begin()
{
    pinMode(l_rpwm_pin, OUTPUT);
    pinMode(l_lpwm_pin, OUTPUT);
    pinMode(l_len_pin, OUTPUT);
    pinMode(l_ren_pin, OUTPUT);

    pinMode(r_rpwm_pin, OUTPUT);
    pinMode(r_lpwm_pin, OUTPUT);
    pinMode(r_len_pin, OUTPUT);
    pinMode(r_ren_pin, OUTPUT);

    stop();
}

void MotorController::driveSingleMotor(int speed, int rpwm, int lpwm, int len, int ren)
{
    speed = constrain(speed, -255, 255);

    digitalWrite(len, HIGH);
    digitalWrite(ren, HIGH);

    if (speed > 0)
    {
        analogWrite(rpwm, speed);
        digitalWrite(lpwm, LOW);
    }
    else if (speed < 0)
    {
        digitalWrite(rpwm, LOW);
        analogWrite(lpwm, -speed);
    }
    else
    {
        analogWrite(rpwm, 0);
        analogWrite(lpwm, 0);
    }
}

void MotorController::driveLeft(int speed)
{
    driveSingleMotor(speed, l_rpwm_pin, l_lpwm_pin, l_len_pin, l_ren_pin);
}

void MotorController::driveRight(int speed)
{
    driveSingleMotor(speed, r_rpwm_pin, r_lpwm_pin, r_len_pin, r_ren_pin);
}

void MotorController::hillBrake()
{
    // To brake the BTS7960, we set EN to HIGH and Both PWM to LOW,
    // or Both PWM to HIGH. Setting both to LOW creates a hard short brake
    digitalWrite(l_len_pin, HIGH);
    digitalWrite(l_ren_pin, HIGH);
    digitalWrite(l_rpwm_pin, LOW);
    digitalWrite(l_lpwm_pin, LOW);

    digitalWrite(r_len_pin, HIGH);
    digitalWrite(r_ren_pin, HIGH);
    digitalWrite(r_rpwm_pin, LOW);
    digitalWrite(r_lpwm_pin, LOW);
}

void MotorController::stop()
{
    digitalWrite(l_rpwm_pin, LOW);
    digitalWrite(l_lpwm_pin, LOW);
    digitalWrite(l_len_pin, LOW);
    digitalWrite(l_ren_pin, LOW);

    digitalWrite(r_rpwm_pin, LOW);
    digitalWrite(r_lpwm_pin, LOW);
    digitalWrite(r_len_pin, LOW);
    digitalWrite(r_ren_pin, LOW);
}
