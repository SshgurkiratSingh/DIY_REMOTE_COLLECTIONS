import re

with open('src/DisplayManager.cpp', 'r') as f:
    content = f.read()

# Patch STATE_SETTINGS items array
old_items = """        const char *items[] = {
            "Add Tgt(AP)", "Deadzone", "Inv X/Y", "Factory Rst",
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
            char buf[32];"""

new_items = """        const char *items[] = {
            "Add Tgt(AP)", "Deadzone", "Inv X/Y", "Factory Rst",
            "Tx Rate", "Rx Mode", "LED Maps", "Out Type", "Feedback"};

        int safeSubIndex = subIndex < 0 ? 0 : (subIndex > 8 ? 8 : subIndex);
        
        int startIdx = safeSubIndex - 3;
        if (startIdx < 0) startIdx = 0;
        if (startIdx > 2) startIdx = 2; // max window offset (9 items - 7 visible = 2)

        for (int i = startIdx; i < startIdx + 7; i++)
        {
            if (i == safeSubIndex)
            {
                display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            }
            else
            {
                display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
            }

            int row = i - startIdx;
            display.setCursor(0, 8 + (row * 8));
            char buf[32];"""
content = content.replace(old_items, new_items)

# Patch STATE_SETTINGS specific item rendering
old_fmt = """            else if (i == 6)
                snprintf(buf, sizeof(buf), "6. LED Map : %s", config.ledMode < 4 ? modes[config.ledMode] : "UNK");
            else
                snprintf(buf, sizeof(buf), "%d. %s", i, items[i]);"""
new_fmt = """            else if (i == 6)
                snprintf(buf, sizeof(buf), "6. LED Map : %s", config.ledMode < 4 ? modes[config.ledMode] : "UNK");
            else if (i == 7) {
                const char *outModes[] = {"LEDS", "VIBRATOR/BUZZ"};
                snprintf(buf, sizeof(buf), "7. Out Type : %s", config.outType < 2 ? outModes[config.outType] : "UNK");
            }
            else if (i == 8) {
                const char *fbModes[] = {"SILENT", "VIB ONLY", "BUZZ ONLY", "BOTH"};
                snprintf(buf, sizeof(buf), "8. Feedback : %s", config.feedbackMode < 4 ? fbModes[config.feedbackMode] : "UNK");
            }
            else
                snprintf(buf, sizeof(buf), "%d. %s", i, items[i]);"""
content = content.replace(old_fmt, new_fmt)

# Patch STATE_EDIT_PARAM render
old_ed = """            else if (subIndex == 6)
                display.printf("Mod: %s", config.ledMode < 4 ? modes[config.ledMode] : "UNK");
            else"""
new_ed = """            else if (subIndex == 6)
                display.printf("Mod: %s", config.ledMode < 4 ? modes[config.ledMode] : "UNK");
            else if (subIndex == 7) {
                const char *outModes[] = {"LEDS", "VIB/BUZ", "UNK"};
                display.printf("Out: %s", config.outType < 2 ? outModes[config.outType] : "UNK");
            }
            else if (subIndex == 8) {
                const char *fbModes[] = {"SILENT", "VIB ONLY", "BUZZ ONLY", "BOTH"};
                display.printf("FB: %s", config.feedbackMode < 4 ? fbModes[config.feedbackMode] : "UNK");
            }
            else"""
content = content.replace(old_ed, new_ed)

with open('src/DisplayManager.cpp', 'w') as f:
    f.write(content)

with open('src/main.cpp', 'r') as f:
    main_c = f.read()

# patch main.cpp
old_case = """      case 6: // LED Maps
      {
        int newLed = (int)sysConfig.ledMode + encDiff;
        if (newLed < 0)
          newLed = LED_MODE_MAX - 1;
        if (newLed >= LED_MODE_MAX)
          newLed = 0;
        sysConfig.ledMode = (uint8_t)newLed;
        break;
      }
        // Add case 3: Channel later
      }
    }"""
new_case = """      case 6: // LED Maps
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
        if (newOut < 0) newOut = OUT_TYPE_MAX - 1;
        if (newOut >= OUT_TYPE_MAX) newOut = 0;
        sysConfig.outType = (uint8_t)newOut;
        break;
      }
      case 8: // Feedback Mode
      {
        int newFb = (int)sysConfig.feedbackMode + encDiff;
        if (newFb < 0) newFb = FB_MODE_MAX - 1;
        if (newFb >= FB_MODE_MAX) newFb = 0;
        sysConfig.feedbackMode = (uint8_t)newFb;
        break;
      }
        // Add case 3: Channel later
      }
    }"""
main_c = main_c.replace(old_case, new_case)

old_wrap = """      else if (fsmState == STATE_SETTINGS)
      {
        if (subIndex < 0)
          subIndex = 6;
        if (subIndex > 6)
          subIndex = 0; // 7 options (0-6)
      }"""
new_wrap = """      else if (fsmState == STATE_SETTINGS)
      {
        if (subIndex < 0)
          subIndex = 8;
        if (subIndex > 8)
          subIndex = 0; // 9 options (0-8)
      }"""
main_c = main_c.replace(old_wrap, new_wrap)

with open('src/main.cpp', 'w') as f:
    f.write(main_c)
print("PATCHED")
