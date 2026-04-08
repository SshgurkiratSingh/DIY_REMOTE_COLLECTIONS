#include <Arduino.h>
#include "Config.h"
#include "InputManager.h"
#include "DisplayManager.h"
#include "NetworkManager.h"
#include "StorageManager.h"
#include "PortalManager.h"
#include "FeedbackManager.h"

InputManager inputManager;
DisplayManager displayManager;
NetworkManager networkManager;
StorageManager storageManager;
PortalManager portalManager;

SystemConfig sysConfig;
struct_message telemetryData;

unsigned long lastSendTime = 0;

MenuState fsmState = STATE_HUD;
bool inSubMenu = false;
int subIndex = 0;
long lastEncoderPos = 0;

char activeTargetName[16] = "No Target";
uint8_t activeTargetMac[6] = {0, 0, 0, 0, 0, 0};
uint8_t activeVerifyKey = 0;

void loadActiveTarget(int index)
{
  int targetCount = storageManager.getTargetCount();
  if (targetCount > 0)
  {
    if (index < 0 || index >= targetCount)
    {
      index = 0;
    }

    TargetNode t = storageManager.getTarget(index);
    strncpy(activeTargetName, t.name, 15);
    activeTargetName[15] = '\0';

    if (networkManager.updateTarget(t.mac))
    {
      memcpy(activeTargetMac, t.mac, 6);
      activeVerifyKey = t.verifyKey;
      storageManager.setLastTargetIndex(index);
    }
    else
    {
      strncpy(activeTargetName, "No Target", 15);
      activeTargetName[15] = '\0';
      memset(activeTargetMac, 0, sizeof(activeTargetMac));
      activeVerifyKey = 0;
    }
  }
  else
  {
    strncpy(activeTargetName, "No Target", 15);
    activeTargetName[15] = '\0';
    memset(activeTargetMac, 0, sizeof(activeTargetMac));
    activeVerifyKey = 0;
  }
}

void setup()
{
  Serial.begin(115200);

  storageManager.begin();
  storageManager.loadConfig(sysConfig);

  if (!displayManager.begin())
  {
    Serial.println("Display Init Failed!");
  }

  inputManager.begin();
  feedbackManager.begin();
  feedbackManager.trigger(FeedbackEvent::STARTUP);

  if (!networkManager.begin(sysConfig))
  {
    Serial.println("ESP-NOW Init Failed!");
  }
  else
  {
    int lastTarget = storageManager.getLastTargetIndex();
    loadActiveTarget(lastTarget); // Load persistent target by default
  }

  lastEncoderPos = inputManager.getEncoderPosition();
  delay(1000); // Intro screen
}

void loop()
{
  if (portalManager.isRunning())
  {
    portalManager.handleClient();
    return; // Halt all normal transmission loops
  }

  feedbackManager.update(sysConfig);
  unsigned long currentMillis = millis();
  unsigned int sendInterval = 1000 / (sysConfig.txRateHz > 0 ? sysConfig.txRateHz : 1);

  // Encoder Navigation Logic
  long currentEnc = inputManager.getEncoderPosition();
  long encDiff = currentEnc - lastEncoderPos;

  if (encDiff != 0)
  {
    if (fsmState == STATE_EDIT_PARAM)
    {
      // Adjust the parameter directly
      switch (subIndex)
      {
      case 1: // Deadzone
      {
        int newDz = (int)sysConfig.deadzone + (encDiff * 5);
        if (newDz < 0)
          newDz = 0;
        if (newDz > 2000)
          newDz = 2000;
        sysConfig.deadzone = (uint16_t)newDz;
        break;
      }
      case 2: // Invert X/Y
      {
        if (encDiff > 0)
          sysConfig.invertX = !sysConfig.invertX;
        else
          sysConfig.invertY = !sysConfig.invertY;
        break;
      }
      case 4: // Tx Rate
      {
        int newRt = (int)sysConfig.txRateHz + (encDiff * 5);
        if (newRt < 5)
          newRt = 5;
        if (newRt > 250)
          newRt = 250;
        sysConfig.txRateHz = (uint8_t)newRt;
        break;
      }
      case 5: // Telemetry Rx
      {
        sysConfig.rxEnabled = !sysConfig.rxEnabled;
        break;
      }
      case 6: // LED Maps
      {
        int newLed = (int)sysConfig.ledMode + encDiff;
        if (newLed < 0)
          newLed = LED_MODE_MAX - 1;
        if (newLed >= LED_MODE_MAX)
          newLed = 0;
        sysConfig.ledMode = (uint8_t)newLed;
        break;
      }
      case 7: // Out Type
      {
        int newOut = (int)sysConfig.outType + encDiff;
        if (newOut < 0)
          newOut = OUT_TYPE_MAX - 1;
        if (newOut >= OUT_TYPE_MAX)
          newOut = 0;
        sysConfig.outType = (uint8_t)newOut;
        break;
      }
      case 8: // Feedback Mode
      {
        int newFb = (int)sysConfig.feedbackMode + encDiff;
        if (newFb < 0)
          newFb = FB_MODE_MAX - 1;
        if (newFb >= FB_MODE_MAX)
          newFb = 0;
        sysConfig.feedbackMode = (uint8_t)newFb;
        break;
      }
        // Add case 3: Channel later
      }
    }
    else if (!inSubMenu)
    {
      // Scroll States (0 to 3)
      int newState = (int)fsmState + encDiff;
      if (newState < 0)
        newState = STATE_EDIT_PARAM - 1; // max top level state
      if (newState >= STATE_EDIT_PARAM)
        newState = 0;
      fsmState = (MenuState)newState;
    }
    else
    {
      // Scroll Sub Index
      subIndex += encDiff;

      if (fsmState == STATE_TARGET_SELECT)
      {
        int maxTargets = storageManager.getTargetCount();
        if (maxTargets == 0)
          subIndex = 0;
        else
        {
          if (subIndex < 0)
            subIndex = maxTargets - 1;
          if (subIndex >= maxTargets)
            subIndex = 0;
        }
      }
      else if (fsmState == STATE_SETTINGS)
      {
        if (subIndex < 0)
          subIndex = 9;
        if (subIndex > 9)
          subIndex = 0; // 10 options (0-9)
      }
    }
    lastEncoderPos = currentEnc;
  }

  // Switch Click Logic
  static bool lastSwState = true;
  static unsigned long lastClickTime = 0;
  bool currentSwState = !digitalRead(PIN_ENC_SW); // pulled up, false is pressed

  if (currentSwState && !lastSwState)
  {
    feedbackManager.trigger(FeedbackEvent::BUTTON_PRESS);
    unsigned long now = millis();
    if (now - lastClickTime < 400)
    {
      // Double Click Action -> Exit settings, go to HUD
      fsmState = STATE_HUD;
      inSubMenu = false;
      storageManager.saveConfig(sysConfig);
      lastClickTime = 0; // Reset
    }
    else
    {
      lastClickTime = now;

      // Single Click Logic
      if (fsmState == STATE_EDIT_PARAM)
      {
        // Save and return to settings menu
        storageManager.saveConfig(sysConfig);
        fsmState = STATE_SETTINGS;
      }
      else if (fsmState == STATE_TARGET_SELECT || fsmState == STATE_SETTINGS)
      {
        if (!inSubMenu)
        {
          // Enter sub menu
          inSubMenu = true;
          subIndex = 0;
        }
        else
        {
          // Trigger action in sub menu
          if (fsmState == STATE_TARGET_SELECT)
          {
            loadActiveTarget(subIndex);
            inSubMenu = false; // Exit sub menu
          }
          else if (fsmState == STATE_SETTINGS)
          {
            if (subIndex == 0)
            { // Add Target (AP Portal)
              displayManager.showAPMode();
              portalManager.startPortal(&storageManager, &displayManager);
              return;
            }
            else if (subIndex == 3)
            { // Factory reset: clear all stored settings/targets then reboot
              storageManager.factoryReset();
              delay(200);
              ESP.restart();
            }
            else if (subIndex == 9)
            { // Haptic Test trigger
              feedbackManager.trigger(FeedbackEvent::CONNECTION_LOST);
            }
            else
            {
              fsmState = STATE_EDIT_PARAM; // Enter Edit Mode
            }
          }
        }
      }
      else if (fsmState == STATE_HUD || fsmState == STATE_TELEMETRY || fsmState == STATE_ABOUT || fsmState == STATE_RX_VIEWER)
      {
        inSubMenu = false; // reset state
      }
    }
  }
  lastSwState = currentSwState;

  // Non-blocking transmission
  if (currentMillis - lastSendTime >= sendInterval)
  {
    lastSendTime = currentMillis;

    bool prevPush1 = telemetryData.push1;
    bool prevPush2 = telemetryData.push2;
    bool prevToggle1 = telemetryData.toggle1;
    bool prevToggle2 = telemetryData.toggle2;

    inputManager.readInputs(&telemetryData, sysConfig);

    // Trigger haptic/audio feedback on any push button press or toggle switch change
    if ((telemetryData.toggle1 != prevToggle1) ||
        (telemetryData.toggle2 != prevToggle2))
    {
      feedbackManager.trigger(FeedbackEvent::BUTTON_PRESS);
    }

    bool anyPushDown = telemetryData.push1 || telemetryData.push2;
    bool prevAnyPushDown = prevPush1 || prevPush2;
    if (anyPushDown != prevAnyPushDown)
    {
      if (anyPushDown)
      {
        feedbackManager.trigger(FeedbackEvent::BUTTON_PRESS);
      }
      else
      {
        feedbackManager.setContinuousVibrator(false, sysConfig);
      }
    }

    if (anyPushDown)
    {
      feedbackManager.setContinuousVibrator(true, sysConfig);
    }

    telemetryData.verifyKey = activeVerifyKey;

    networkManager.sendData(&telemetryData);

    // When in target select sub menu, show the target being previewed
    char displayTarget[16];
    strncpy(displayTarget, activeTargetName, 15);
    displayTarget[15] = '\0';
    if (fsmState == STATE_TARGET_SELECT && inSubMenu)
    {
      if (storageManager.getTargetCount() > 0)
      {
        TargetNode t = storageManager.getTarget(subIndex);
        strncpy(displayTarget, t.name, 15);
        displayTarget[15] = '\0';
      }
      else
      {
        strncpy(displayTarget, "<EMPTY>", 15);
        displayTarget[15] = '\0';
      }
    }

    rx_message rxData = networkManager.getLastRxData();
    displayManager.update(&telemetryData, networkManager.getLastStatus(), fsmState, subIndex, displayTarget, sysConfig, &rxData);
  }
}
