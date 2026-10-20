/**
 * ============================================================================
 *  ESP-NOW Receiver v2.5 (Special Edition)
 *  L298N Motor Driver + Dual WS2812B Accent Strips + WS2812B Brake Lights
 * ============================================================================
 *  Compatible with: Transmitter v2.0 (ESP-NOW OLED Remote)
 *  Board:           ESP32 DevKit
 * ============================================================================
 */

#include <Arduino.h>
#include "Config.h"
#include "MotorDriver.h"
#include "Network.h"
#include "EyeMatrix.h"

MotorDriver motors;
Network     network;
EyeMatrix   eyes;

void setup() {
    Serial.begin(115200);
    delay(300);

    Serial.println("====================================");
    Serial.printf("  struct_message size: %d bytes\n", sizeof(struct_message));
    Serial.println("  ESP-NOW Receiver v2.5 Special Edition");
    Serial.println("  L298N + WS2812B Accent + Brake Lights");
    Serial.println("====================================");

    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(STATUS_LED, LOW);

    motors.begin();
    eyes.begin();

    if (!network.begin()) {
        Serial.println("System Halted due to Network failure.");
        while (true) delay(100);
    }

    Serial.println("Ready. Waiting for transmitter...");
}

void loop() {
    if (!network.isConnected()) {
        motors.stop();
        digitalWrite(STATUS_LED, LOW);
        eyes.displaySignalLoss();
        delay(15);
        return;
    }

    digitalWrite(STATUS_LED, HIGH);

    const struct_message& data = network.getData();

    static bool lastPush1 = false;
    static int  localLedMode = 0;

    bool motorBoost = false;

    if (data.toggle2) {
        if (data.push1 && !lastPush1)
            localLedMode = (localLedMode + 1) % TOTAL_LED_MODES;
    } else {
        if (data.push1) motorBoost = true;
        localLedMode = data.addrLedMode;
    }
    lastPush1 = data.push1;

    int activeLedMode = data.toggle2 ? localLedMode : data.addrLedMode;

    // Map pot to brightness (floor at MIN_BRIGHTNESS)
    uint8_t brightness = (uint8_t)map(data.potValue, 0, 4095, MIN_BRIGHTNESS, 255);
    eyes.setBrightness(brightness);

    motors.mixDrive(data.joyY, data.joyX, data.potValue, motorBoost);
    motors.update();

    if (network.hasNewLedMode())
        Serial.printf("Remote LED Mode -> %d\n", data.addrLedMode);

    eyes.update(activeLedMode, motors.getCurrentSpeedA(), motors.getTargetTurn());

    network.update(activeLedMode, data.toggle1);

    yield();
}
