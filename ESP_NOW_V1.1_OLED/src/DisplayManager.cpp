#include "DisplayManager.h"
#include <WiFi.h>

bool DisplayManager::begin()
{
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR))
    {
        return false;
    }

    pinMode(PIN_LED_YELLOW, OUTPUT);
    pinMode(PIN_LED_GREEN, OUTPUT);
    digitalWrite(PIN_LED_YELLOW, LOW);
    digitalWrite(PIN_LED_GREEN, LOW);

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("System Ready!");
    display.display();
    return true;
}

void DisplayManager::showAPMode()
{
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("> Captive Portal <");
    display.println("WIFI: ESP-NOW-REMOTE");
    display.println("PASS: 12345678");
    display.println("Navigate 192.168.4.1");
    display.display();
}

void DisplayManager::update(const struct_message *data, bool sendStatus, MenuState state, int subIndex, const char *targetName, const SystemConfig &config, const rx_message *rxData)
{
    display.clearDisplay();
    display.setTextSize(1);

    switch (state)
    {
    case STATE_HUD:
    {
        display.fillRect(0, 0, SCREEN_WIDTH, 12, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
        display.setCursor(2, 2);
        if (targetName && targetName[0] != '\0')
        {
            display.print(targetName);
        }
        else
        {
            display.print("NO TARGET");
        }
        display.setTextColor(SSD1306_WHITE);

        int wX = (data->joyX * 90) / 4095;
        int wY = (data->joyY * 90) / 4095;

        // X Bar
        display.setCursor(0, 20);
        display.print("X");
        display.drawRect(10, 20, 90, 8, SSD1306_WHITE);
        display.fillRect(10, 20, wX, 8, SSD1306_WHITE);

        // Y Bar
        display.setCursor(0, 36);
        display.print("Y");
        display.drawRect(10, 36, 90, 8, SSD1306_WHITE);
        display.fillRect(10, 36, wY, 8, SSD1306_WHITE);

        // Buttons
        int radius = 4;
        if (data->push1)
            display.drawCircle(115, 24, radius, SSD1306_WHITE);
        else
            display.fillCircle(115, 24, radius, SSD1306_WHITE);

        if (data->push2)
            display.drawCircle(115, 40, radius, SSD1306_WHITE);
        else
            display.fillCircle(115, 40, radius, SSD1306_WHITE);

        // Rx & Tx
        display.setCursor(0, 52);
        if (config.rxEnabled && rxData)
        {
            display.printf("RX: %d %d %d", rxData->data1, rxData->data2, rxData->data3);
        }
        else
        {
            display.print(sendStatus ? "TX: OK" : "TX: FAIL");
            display.print(config.rxEnabled ? "   RX: ON" : "   RX: OFF");
        }
        break;
    }

    case STATE_TELEMETRY:
    {
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.printf("VRX(35): %04d\n\n", data->joyX);
        display.printf("VRY(33): %04d\n\n", data->joyY);
        display.printf("POT(32): %04d\n\n", data->potValue);
        display.printf("Sw: T(%d,%d) P(%d,%d)", data->toggle1, data->toggle2, data->push1, data->push2);
        break;
    }

    case STATE_RX_VIEWER:
    {
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("--- RX VIEWER ---");

        display.setCursor(0, 24);
        display.setTextSize(2);
        if (config.rxEnabled && rxData)
        {
            display.printf("%03d %03d %03d", rxData->data1, rxData->data2, rxData->data3);
        }
        else
        {
            display.setTextSize(1);
            display.println("RX disabled or");
            display.println("No data yet");
        }
        break;
    }

    case STATE_TARGET_SELECT:
    {
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("--- TARGET SELECT ---");
        display.setCursor(0, 20);
        display.printf("Index: %d\n", subIndex);

        display.setCursor(0, 36);
        display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
        String nameStr = "> ";
        nameStr += (targetName && targetName[0] != '\0') ? targetName : "<EMPTY>";
        while (nameStr.length() < 21)
            nameStr += " "; // Pad to cover width
        display.print(nameStr);

        display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
        display.setCursor(0, 52);
        display.println("Click SW to Connect");
        break;
    }

    case STATE_SETTINGS:
    {
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("--- SETTINGS ---");

        const char *items[] = {
            "Add Tgt(AP)", "Deadzone", "Inv X/Y", "Channel",
            "Tx Rate", "Rx Mode", "LED Maps"};

        int safeSubIndex = subIndex < 0 ? 0 : (subIndex > 6 ? 6 : subIndex);

        for (int i = 0; i < 7; i++)
        {
            if (i == safeSubIndex)
            {
                display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            }
            else
            {
                display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
            }

            // y spreads mathematically from row 8 down to 56 fitting perfectly in 64px display
            display.setCursor(0, 8 + (i * 8));
            char buf[32];
            if (i == 1)
                snprintf(buf, sizeof(buf), "1. Deadzone: %d", config.deadzone);
            else if (i == 2)
                snprintf(buf, sizeof(buf), "2. Inv X/Y : %d/%d", config.invertX, config.invertY);
            else if (i == 4)
                snprintf(buf, sizeof(buf), "4. Tx Rate : %dHz", config.txRateHz);
            else if (i == 5)
                snprintf(buf, sizeof(buf), "5. Rx Mode : %s", config.rxEnabled ? "ON" : "OFF");
            else if (i == 6)
            {
                const char *modes[] = {"TX/RX", "LINK", "ACTION", "STEALTH"};
                snprintf(buf, sizeof(buf), "6. LED Map : %s", config.ledMode < 4 ? modes[config.ledMode] : "UNK");
            }
            else
                snprintf(buf, sizeof(buf), "%d. %s", i, items[i]);

            String padded = String(buf);
            while (padded.length() < 21)
                padded += " ";
            display.print(padded);
        }
        display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
        break;
    }

    case STATE_ABOUT:
    {
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("--- ABOUT ---");

        display.setCursor(0, 16);
        display.println("ESP NOW Controller");

        display.setCursor(0, 32);
        display.print("Fw: ");
        display.println(FIRMWARE_VERSION);

        display.setCursor(0, 48);
        display.print("MAC: ");
        display.println(WiFi.macAddress());
        break;
    }

    case STATE_EDIT_PARAM:
    {
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("--- EDIT PARAM ---");

        display.setCursor(0, 24);
        display.setTextSize(2);

        if (subIndex == 1)
        {
            display.printf("DZ: %d", config.deadzone);
        }
        else if (subIndex == 2)
        {
            display.printf("X:%d Y:%d", config.invertX, config.invertY);
        }
        else if (subIndex == 4)
        {
            display.printf("Tx: %dHz", config.txRateHz);
        }
        else if (subIndex == 5)
        {
            display.printf("Rx: %s", config.rxEnabled ? "ON" : "OFF");
        }
        else if (subIndex == 6)
        {
            const char *modes[] = {"TX/RX", "LINK", "ACTION", "STEALTH"};
            display.printf("Mod: %s", config.ledMode < 4 ? modes[config.ledMode] : "UNK");
        }
        else
        {
            display.println("NOT IMPL");
        }

        display.setTextSize(1);
        display.setCursor(0, 52);
        display.println("Click SW to Save");
        break;
    }

    default:
        break;
    }

    display.display();

    // Status LEDs
    if (config.ledMode == LED_STEALTH)
    {
        digitalWrite(PIN_LED_GREEN, LOW);
        digitalWrite(PIN_LED_YELLOW, LOW);
    }
    else if (config.ledMode == LED_ACTION)
    {
        bool activeInput = data->push1 || data->push2 || data->toggle1 || data->toggle2;
        digitalWrite(PIN_LED_GREEN, activeInput ? HIGH : LOW);
        digitalWrite(PIN_LED_YELLOW, LOW);
    }
    else if (config.ledMode == LED_LINK_STATE)
    {
        // Basic link tracking: if last TX was ok, we are linked.
        digitalWrite(PIN_LED_GREEN, sendStatus ? HIGH : LOW);
        digitalWrite(PIN_LED_YELLOW, sendStatus ? LOW : HIGH);
    }
    else
    { // LED_TX_RX Mode (or undefined)
        digitalWrite(PIN_LED_GREEN, sendStatus ? HIGH : LOW);
        digitalWrite(PIN_LED_YELLOW, sendStatus ? LOW : HIGH);
    }
}
