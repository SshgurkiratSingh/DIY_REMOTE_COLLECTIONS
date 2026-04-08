import re

with open('src/DisplayManager.cpp', 'r') as f:
    content = f.read()

# Patch STATE_SETTINGS items array
old_items = """        const char *items[] = {
            "Add Tgt(AP)", "Deadzone", "Inv X/Y", "Factory Rst",
            "Tx Rate", "Rx Mode", "LED Maps", "Out Type", "Feedback"};

        int safeSubIndex = subIndex < 0 ? 0 : (subIndex > 8 ? 8 : subIndex);
        
        int startIdx = safeSubIndex - 3;
        if (startIdx < 0) startIdx = 0;
        if (startIdx > 2) startIdx = 2; // max window offset (9 items - 7 visible = 2)"""

new_items = """        const char *items[] = {
            "Add Tgt(AP)", "Deadzone", "Inv X/Y", "Factory Rst",
            "Tx Rate", "Rx Mode", "LED Maps", "Out Type", "Feedback", "Test Haptic"};

        int safeSubIndex = subIndex < 0 ? 0 : (subIndex > 9 ? 9 : subIndex);
        
        int startIdx = safeSubIndex - 3;
        if (startIdx < 0) startIdx = 0;
        if (startIdx > 3) startIdx = 3; // max window offset (10 items - 7 visible = 3)"""

content = content.replace(old_items, new_items)

# Patch STATE_SETTINGS specific item rendering
old_fmt = """            else if (i == 8) {
                const char *fbModes[] = {"SILENT", "VIB ONLY", "BUZZ ONLY", "BOTH"};
                snprintf(buf, sizeof(buf), "8. Feedback : %s", config.feedbackMode < 4 ? fbModes[config.feedbackMode] : "UNK");
            }
            else
                snprintf(buf, sizeof(buf), "%d. %s", i, items[i]);"""

new_fmt = """            else if (i == 8) {
                const char *fbModes[] = {"SILENT", "VIB ONLY", "BUZZ ONLY", "BOTH"};
                snprintf(buf, sizeof(buf), "8. Feedback : %s", config.feedbackMode < 4 ? fbModes[config.feedbackMode] : "UNK");
            }
            else if (i == 9) {
                snprintf(buf, sizeof(buf), "9. Test Feedback");
            }
            else
                snprintf(buf, sizeof(buf), "%d. %s", i, items[i]);"""

content = content.replace(old_fmt, new_fmt)

with open('src/DisplayManager.cpp', 'w') as f:
    f.write(content)

with open('src/main.cpp', 'r') as f:
    main_c = f.read()

old_wrap = """        if (subIndex < 0)
          subIndex = 8;
        if (subIndex > 8)
          subIndex = 0; // 9 options (0-8)"""

new_wrap = """        if (subIndex < 0)
          subIndex = 9;
        if (subIndex > 9)
          subIndex = 0; // 10 options (0-9)"""
main_c = main_c.replace(old_wrap, new_wrap)

old_click = """              else if (subIndex == 3)
              { // Factory reset: clear all stored settings/targets then reboot
                storageManager.factoryReset();
                delay(200);
                ESP.restart();
              }
              else
              {
                fsmState = STATE_EDIT_PARAM; // Enter Edit Mode
              }"""

new_click = """              else if (subIndex == 3)
              { // Factory reset: clear all stored settings/targets then reboot
                storageManager.factoryReset();
                delay(200);
                ESP.restart();
              }
              else if (subIndex == 9)
              { // Haptic Test
                feedbackManager.trigger(FeedbackEvent::CONNECTION_LOST);
                // Stays in settings menu, does not go to STATE_EDIT_PARAM
              }
              else
              {
                fsmState = STATE_EDIT_PARAM; // Enter Edit Mode
              }"""
main_c = main_c.replace(old_click, new_click)

with open('src/main.cpp', 'w') as f:
    f.write(main_c)
print("Test option added successfully")
