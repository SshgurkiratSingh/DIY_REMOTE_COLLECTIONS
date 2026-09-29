/**
 * ============================================================================
 *  ESP-NOW Receiver v2.0-led (EYE MATRIX EDITION)
 *  L298N Motor Driver + Dual WS2812B LED Strips (10 LEDs each)
 * ============================================================================
 *
 *  Compatible with: Transmitter v2.0 (ESP-NOW OLED Remote)
 *  Board:           ESP32 DevKit
 *  Motor Driver:    L298N (dual H-bridge)
 *  LED Strip:       2x WS2812B (10 LEDs per eye, GRB order)
 *
 *  Eye Layout (physical):
 *    Line 1  : LEDs 0, 1, 2
 *    Hidden  : LED  3        ← always off (routing LED, never visible)
 *    Line 2  : LEDs 4, 5, 6
 *    Hidden  : LED  7        ← always off (routing LED, never visible)
 *    Line 3  : LEDs 8, 9
 *
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
    Serial.println("  ESP-NOW Receiver v2.0-led (Rewrite)");
    Serial.println("  L298N + Dual WS2812B Eye Matrix");
    Serial.println("====================================");

    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(STATUS_LED, LOW);

    motors.begin();
    eyes.begin();
    
    if (!network.begin()) {
        Serial.println("System Halted due to Network failure.");
        while(true) delay(100);
    }
    
    Serial.println("Ready. Waiting for transmitter...");
}

void loop() {
    // 1. Safety Check
    if (!network.isConnected()) {
        motors.stop();
        digitalWrite(STATUS_LED, LOW);
        eyes.displaySignalLoss();
        // Limit loop rate slightly while disconnected to save power, 
        // but FastLED beatsin8 requires somewhat frequent updates.
        delay(15); 
        return;
    }
    
    // Connected
    digitalWrite(STATUS_LED, HIGH);
    
    const struct_message& data = network.getData();

    // 2. Brightness Update
    uint8_t brightness = (uint8_t)max((int)MIN_BRIGHTNESS, (int)map(data.potValue, 0, 4095, 0, 255));
    eyes.setBrightness(brightness);
    
    // 3. Motor Update
    if (data.toggle2) {
        motors.mixDrive(data.joyY, data.joyX);
        motors.update(); // handles smooth ramping internally (at 50Hz)
    } else {
        motors.stop();
    }
    
    // 4. LED Animation Update
    if (network.hasNewLedMode()) {
        Serial.printf("LED Mode -> %d\n", data.addrLedMode);
    }
    
    // The EyeMatrix handles the internal high framerate updates seamlessly
    eyes.update(data.addrLedMode, data.toggle1, motors.getTargetFwd(), motors.getTargetTurn());
    
    // 5. Telemetry Feedback Update
    network.update(data.addrLedMode, data.toggle1);
    
    // Let the loop run as fast as possible for buttery smooth FastLED animations.
    // yield() helps prevent watchdog resets.
    yield();
}
