import re

with open("include/FeedbackManager.h", "r") as f:
    text = f.read()

text = text.replace("void stopAll();", "void stopAll();\n    void setContinuousVibrator(bool on, const SystemConfig &config);")

with open("include/FeedbackManager.h", "w") as f:
    f.write(text)

with open("src/FeedbackManager.cpp", "r") as f:
    text = f.read()

setter_code = """
void FeedbackManager::setContinuousVibrator(bool on, const SystemConfig &config) {
    if (config.outType != OUT_VIB_BUZZ) return;
    _vibEnabled = (config.feedbackMode == FB_VIB_ONLY || config.feedbackMode == FB_VIB_BUZ);
    // Don't interrupt other events with stop calls unless we have to, but we just set the pin directly.
    setVibrator(on);
}
"""

text = text.replace("void FeedbackManager::setBuzzer(uint32_t freq) {", setter_code + "\nvoid FeedbackManager::setBuzzer(uint32_t freq) {")

with open("src/FeedbackManager.cpp", "w") as f:
    f.write(text)
