import re

with open("src/main.cpp", "r") as f:
    text = f.read()

old_block = """    if ((telemetryData.push1 && !prevPush1) || 
        (telemetryData.push2 && !prevPush2) ||
        (telemetryData.toggle1 != prevToggle1) || 
        (telemetryData.toggle2 != prevToggle2)) 
    {
        feedbackManager.trigger(FeedbackEvent::BUTTON_PRESS);
    }"""

new_block = """    if ((telemetryData.toggle1 != prevToggle1) || 
        (telemetryData.toggle2 != prevToggle2)) 
    {
        feedbackManager.trigger(FeedbackEvent::BUTTON_PRESS);
    }

    bool anyPushDown = telemetryData.push1 || telemetryData.push2;
    bool prevAnyPushDown = prevPush1 || prevPush2;
    if (anyPushDown != prevAnyPushDown) {
        if (anyPushDown) {
            feedbackManager.trigger(FeedbackEvent::BUTTON_PRESS); // still beep if needed
        }
        feedbackManager.setContinuousVibrator(anyPushDown, sysConfig);
    }
"""

if old_block in text:
    text = text.replace(old_block, new_block)
    with open("src/main.cpp", "w") as f:
        f.write(text)
    print("Replaced button press block.")
else:
    print("Block not found!")
