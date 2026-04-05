with open("src/main.cpp", "r") as f:
    text = f.read()

old_block = """    bool anyPushDown = telemetryData.push1 || telemetryData.push2;
    bool prevAnyPushDown = prevPush1 || prevPush2;
    if (anyPushDown != prevAnyPushDown) {
        if (anyPushDown) {
            feedbackManager.trigger(FeedbackEvent::BUTTON_PRESS); // still beep if needed
        }
        feedbackManager.setContinuousVibrator(anyPushDown, sysConfig);
    }"""

new_block = """    bool anyPushDown = telemetryData.push1 || telemetryData.push2;
    bool prevAnyPushDown = prevPush1 || prevPush2;
    if (anyPushDown != prevAnyPushDown) {
        if (anyPushDown) {
            feedbackManager.trigger(FeedbackEvent::BUTTON_PRESS);
        } else {
            feedbackManager.setContinuousVibrator(false, sysConfig);
        }
    }
    
    if (anyPushDown) {
        feedbackManager.setContinuousVibrator(true, sysConfig);
    }"""

if old_block in text:
    text = text.replace(old_block, new_block)
    with open("src/main.cpp", "w") as f:
        f.write(text)
    print("Fixed continuous vibrator in main.")
else:
    print("Block not found!")
