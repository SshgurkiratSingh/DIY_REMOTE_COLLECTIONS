#include "StorageManager.h"

void StorageManager::begin()
{
    preferences.begin("sys", false);
}

void StorageManager::loadConfig(SystemConfig &config)
{
    config.deadzone = preferences.getUShort("deadzone", 150);
    config.centerX = preferences.getUShort("centerX", 2048);
    config.centerY = preferences.getUShort("centerY", 2048);
    config.invertX = preferences.getBool("invertX", false);
    config.invertY = preferences.getBool("invertY", false);
    config.txRateHz = preferences.getUChar("txRateHz", 50);
    config.rxEnabled = preferences.getBool("rxEnabled", false);
    config.ledMode = preferences.getUChar("ledMode", LED_TX_RX);
    config.outType = preferences.getUChar("outType", OUT_LEDS);
    config.feedbackMode = preferences.getUChar("fdbkMode", FB_VIB_BUZ);
}

void StorageManager::saveConfig(const SystemConfig &config)
{
    preferences.putUShort("deadzone", config.deadzone);
    preferences.putUShort("centerX", config.centerX);
    preferences.putUShort("centerY", config.centerY);
    preferences.putBool("invertX", config.invertX);
    preferences.putBool("invertY", config.invertY);
    preferences.putUChar("txRateHz", config.txRateHz);
    preferences.putBool("rxEnabled", config.rxEnabled);
    preferences.putUChar("ledMode", config.ledMode);
    preferences.putUChar("outType", config.outType);
    preferences.putUChar("fdbkMode", config.feedbackMode);
}

void StorageManager::factoryReset()
{
    // Clears all keys in the "sys" namespace (config, targets, and UI state).
    preferences.clear();
}

void StorageManager::addTarget(const char *name, const uint8_t *mac)
{
    int count = preferences.getInt("tgt_cnt", 0);
    if (count >= MAX_TARGETS)
    {
        count = MAX_TARGETS - 1; // Overwrite last if full
    }

    char key[16];
    sprintf(key, "tgt_mac_%d", count);
    preferences.putBytes(key, mac, 6);

    sprintf(key, "tgt_name_%d", count);
    preferences.putString(key, name);

    preferences.putInt("tgt_cnt", count + 1);
}

int StorageManager::getTargetCount()
{
    return preferences.getInt("tgt_cnt", 0);
}

TargetNode StorageManager::getTarget(int index)
{
    TargetNode node;
    memset(node.mac, 0, 6);
    node.name[0] = '\0';
    node.active = false;

    int count = getTargetCount();
    if (index >= 0 && index < count)
    {
        char key[16];
        sprintf(key, "tgt_mac_%d", index);
        preferences.getBytes(key, node.mac, 6);

        sprintf(key, "tgt_name_%d", index);
        String name = preferences.getString(key, "");
        strncpy(node.name, name.c_str(), 15);
        node.name[15] = '\0';
    }

    return node;
}

int StorageManager::getLastTargetIndex()
{
    return preferences.getInt("last_tgt", 0);
}

void StorageManager::setLastTargetIndex(int index)
{
    preferences.putInt("last_tgt", index);
}
