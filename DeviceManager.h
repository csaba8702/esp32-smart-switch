#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <Arduino.h>
#include <time.h>
#include "DeviceTypes.h"
#include "EepromManager.h" 

#define RELAY_COUNT 4

static DeviceConfig DEVICE_CONFIG[RELAY_COUNT] = {
    {1, "Rele 1", DeviceType::GENERIC, 16, true, ModuleType::NONE},
    {2, "Rele 2", DeviceType::GENERIC, 17, true, ModuleType::NONE},
    {3, "Rele 3", DeviceType::GENERIC, 18, true, ModuleType::NONE},
    {4, "Rele 4", DeviceType::GENERIC, 19, true, ModuleType::NONE},
};

// Fix méretű karakterpufferek a relék neveinek
static char relayNameBuffers[RELAY_COUNT][32] = {
    "Rele 1", "Rele 2", "Rele 3", "Rele 4"
};

class DeviceManager {
private:
    bool     relayStates[RELAY_COUNT]     = {false};
    uint32_t relayStartTimes[RELAY_COUNT] = {0};
    EepromManager* eeprom = nullptr;

    int findIndex(uint8_t id) {
        for (int i = 0; i < RELAY_COUNT; i++) {
            if (DEVICE_CONFIG[i].id == id) return i;
        }
        return -1;
    }

public:
    DeviceManager() = default;

    void setEeprom(EepromManager& em) { 
        eeprom = &em; 
    }

    void begin() {
        for (int i = 0; i < RELAY_COUNT; i++) {
            pinMode(DEVICE_CONFIG[i].relayPin, OUTPUT);
            
            if (eeprom != nullptr && eeprom->isValid()) {
                // Relé nevének betöltése EEPROM-ból
                String savedName = eeprom->loadRelayName(DEVICE_CONFIG[i].id);
                if (savedName.length() > 0) {
                    strncpy(relayNameBuffers[i], savedName.c_str(), 31);
                    relayNameBuffers[i][31] = '\0';
                    DEVICE_CONFIG[i].name = relayNameBuffers[i];
                }
                
                // Relé utolsó állapotának visszaállítása
                bool savedState = eeprom->loadRelayState(DEVICE_CONFIG[i].id);
                setRelay(DEVICE_CONFIG[i].id, savedState);
                
                // Futási idő bázisának betöltése
                relayStartTimes[i] = eeprom->loadRelayStartTime(DEVICE_CONFIG[i].id);
            } else {
                setRelay(DEVICE_CONFIG[i].id, false);
            }
        }
    }

    bool setRelay(uint8_t id, bool state) {
        int idx = findIndex(id);
        if (idx < 0) return false;

        relayStates[idx] = state;
        bool pinLevel = DEVICE_CONFIG[idx].relayActiveLOW ? state : !state;
        digitalWrite(DEVICE_CONFIG[idx].relayPin, pinLevel);

        if (eeprom != nullptr) {
            eeprom->saveRelayState(id, state);
        }

        if (state) {
            if (relayStartTimes[idx] == 0) {
                relayStartTimes[idx] = (uint32_t)time(nullptr);
                if (eeprom != nullptr) eeprom->saveRelayStartTime(id, relayStartTimes[idx]);
            }
        } else {
            relayStartTimes[idx] = 0;
            if (eeprom != nullptr) eeprom->saveRelayStartTime(id, 0);
        }
        return true;
    }

    bool toggleRelay(uint8_t id) {
        int idx = findIndex(id);
        if (idx < 0) return false;
        return setRelay(id, !relayStates[idx]);
    }

    bool renameRelay(uint8_t id, const String& name) {
        int idx = findIndex(id);
        if (idx < 0) return false;
        
        strncpy(relayNameBuffers[idx], name.c_str(), 31);
        relayNameBuffers[idx][31] = '\0';
        DEVICE_CONFIG[idx].name = relayNameBuffers[idx];
        
        if (eeprom != nullptr) {
            eeprom->saveRelayName(id, name);
        }
        return true;
    }

    bool getState(uint8_t id) {
        int idx = findIndex(id);
        if (idx < 0) return false;
        return relayStates[idx];
    }

    const char* getName(uint8_t id) {
        int idx = findIndex(id);
        if (idx < 0) return "Ismeretlen";
        return relayNameBuffers[idx];
    }

    uint32_t getRelayStartTime(uint8_t id) {
        int idx = findIndex(id);
        if (idx < 0) return 0;
        return relayStartTimes[idx];
    }

    uint32_t getUptime(uint8_t id, uint32_t currentEpoch) {
        int idx = findIndex(id);
        if (idx < 0 || !relayStates[idx] || relayStartTimes[idx] == 0) return 0;
        if (currentEpoch < relayStartTimes[idx]) return 0;
        return currentEpoch - relayStartTimes[idx];
    }
};

#endif // DEVICE_MANAGER_H