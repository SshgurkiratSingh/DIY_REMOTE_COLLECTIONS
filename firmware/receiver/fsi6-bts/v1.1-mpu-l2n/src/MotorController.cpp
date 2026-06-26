#include "MotorController.h"

MotorController::MotorController(int l_en, int l_in1, int l_in2,
                                 int r_en, int r_in1, int r_in2)
{
    l_en_pin = l_en;
    l_in1_pin = l_in1;
    l_in2_pin = l_in2;

    r_en_pin = r_en;
    r_in1_pin = r_in1;
    r_in2_pin = r_in2;
}

void MotorController::begin()
{
    pinMode(l_en_pin, OUTPUT);
    pinMode(l_in1_pin, OUTPUT);
    pinMode(l_in2_pin, OUTPUT);

    pinMode(r_en_pin, OUTPUT);
    pinMode(r_in1_pin, OUTPUT);
    pinMode(r_in2_pin, OUTPUT);

    stop();
}

void MotorController::driveSingleMotor(int speed, int en, int in1, int in2)
{
    speed = constrain(speed, -255, 255);

    if (speed > 0)
    {
        digitalWrite(in1, HIGH);
        digitalWrite(in2, LOW);
        analogWrite(en, speed);
    }
    else if (speed < 0)
    {
        digitalWrite(in1, LOW);
        digitalWrite(in2, HIGH);
        analogWrite(en, -speed);
    }
    else
    {
        digitalWrite(in1, LOW);
        digitalWrite(in2, LOW);
        analogWrite(en, 0);
    }
}

void MotorController::driveLeft(int speed)
{
    driveSingleMotor(speed, l_en_pin, l_in1_pin, l_in2_pin);
}

void MotorController::driveRight(int speed)
{
    driveSingleMotor(speed, r_en_pin, r_in1_pin, r_in2_pin);
}

void MotorController::hillBrake()
{
    digitalWrite(l_in1_pin, HIGH);
    digitalWrite(l_in2_pin, HIGH);
    analogWrite(l_en_pin, 255);

    digitalWrite(r_in1_pin, HIGH);
    digitalWrite(r_in2_pin, HIGH);
    analogWrite(r_en_pin, 255);
}

void MotorController::stop()
{
    digitalWrite(l_in1_pin, LOW);
    digitalWrite(l_in2_pin, LOW);
    analogWrite(l_en_pin, 0);

    digitalWrite(r_in1_pin, LOW);
    digitalWrite(r_in2_pin, LOW);
    analogWrite(r_en_pin, 0);
}
