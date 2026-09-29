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

    // Static states for edge detection and local overrides
    static bool lastPush1 = false;
    static int localLedMode = 0;

    bool currentPush1 = data.push1;
    bool motorBoost = false;

    if (data.toggle2) {
        // Toggle 2 = 1 (LED mode bypass)
        if (currentPush1 && !lastPush1) {
            localLedMode = (localLedMode + 1) % TOTAL_LED_MODES;
        }
    } else {
        // Toggle 2 = 0 (Ramp motor / Boost)
        if (currentPush1) {
            motorBoost = true;
        }
        localLedMode = data.addrLedMode;
    }
    
    lastPush1 = currentPush1;
    
    int activeLedMode = (data.toggle2) ? localLedMode : data.addrLedMode;

    // 2. Motor Update (Speed mapped by potValue, push1 boosts)
    motors.mixDrive(data.joyY, data.joyX, data.potValue, motorBoost);
    motors.update(); // handles smooth ramping internally (at 50Hz)
    
    // 3. LED Animation Update
    if (network.hasNewLedMode()) {
        Serial.printf("Remote LED Mode -> %d\n", data.addrLedMode);
    }
    
    // The EyeMatrix handles the internal high framerate updates seamlessly
    eyes.update(activeLedMode);
    
    // 4. Telemetry Feedback Update
    network.update(activeLedMode, data.toggle1);
    
    // Let the loop run as fast as possible for buttery smooth FastLED animations.
    // yield() helps prevent watchdog resets.
    yield();
}
