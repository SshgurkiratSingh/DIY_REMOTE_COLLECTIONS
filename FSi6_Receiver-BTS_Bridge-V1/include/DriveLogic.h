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

  void applyTankDrive(int ch1, int ch2);
  void applyArcadeDrive(int ch1, int ch2, float speedScale, bool burstMode);
  void applyDynamicSpeedDrive(int ch1, int ch2, float speedScale, bool burstMode);
};

#endif // DRIVE_LOGIC_H
