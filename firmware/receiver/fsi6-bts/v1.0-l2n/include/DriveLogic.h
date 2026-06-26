#ifndef DRIVE_LOGIC_H
#define DRIVE_LOGIC_H

#include "MotorController.h"
#include "Receiver.h"

class DriveLogic
{
public:
  DriveLogic(MotorController *mCtrl);

  void update(ReceiverData input);

private:
  MotorController *motors;

  float targetYaw;
  float homeYaw;
  bool isHeadingHoldActive;

  void applyTankDrive(int ch1, int ch2);
  void applyHeadlessDrive(int ch1, int ch2);
  void applyFullAssistDrive(int ch1, int ch2);

  float normalizeAngle(float angle);
};

#endif // DRIVE_LOGIC_H
