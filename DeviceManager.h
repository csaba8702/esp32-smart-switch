#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <Arduino.h>
#include "DeviceTypes.h"
#include "EepromManager.h"

#define RELAY_COUNT 4

// -----------------------------------------------
// IDE DEFINIÁLD A RELÉKET
// id, név, típus, GPIO pin, activeLOW, modul
// -----------------------------------------------
static const DeviceConfig DEVICE_CONFIG[RELAY_COUNT] = {
    {1, "Relé 1", DeviceType::GENERIC, 16, true, ModuleType::NONE},
    {2, "Relé 2", DeviceType::GENERIC, 17, true, ModuleType::NONE},
    {3, "Relé 3", DeviceType::GENERIC, 18, true, ModuleType::NONE},
    {4, "Relé 4", DeviceType::GENERIC, 19, true, ModuleType::NONE},
};

class DeviceManager {
private:
    bool relayStates[RELAY_COUNT] = {false};
    EepromManager* eeprom = nullptr;

    int findIndex(uint8_t id) {
        for (int i = 0; i < RELAY_COUNT; i++) {
            if (DEVICE_CONFIG[i].id == id) return i;
        }
        return -1;
    }

    void applyRelay(int index, bool state) {
        bool pinState = DEVICE_CONFIG[index].relayActiveLOW ? !state : state;
        digitalWrite(DEVICE_CONFIG[index].relayPin, pinState ? LOW : HIGH);
        relayStates[index] = state;
        Serial.printf("[Device] %s -> %s\n", DEVICE_CONFIG[index].name, state ? "BE" : "KI");
    }

public:
    void setEeprom(EepromManager& em) {
        eeprom = &em;
    }

    void begin() {
        for (int i = 0; i < RELAY_COUNT; i++) {
            pinMode(DEVICE_CONFIG[i].relayPin, OUTPUT);

            // EEPROM-ból töltjük be az utolsó állapotot ha van
            bool savedState = false;
            if (eeprom != nullptr) {
                savedState = eeprom->loadRelayState(DEVICE_CONFIG[i].id);
            }
            applyRelay(i, savedState);
        }
        Serial.println("[DeviceManager] Inicializálva");
    }

    void handle() {
        // Skeleton: itt lesz majd az automatizmus logika
    }

    bool setRelay(uint8_t id, bool state) {
        int idx = findIndex(id);
        if (idx < 0) {
            Serial.printf("[Device] Ismeretlen id: %d\n", id);
            return false;
        }
        applyRelay(idx, state);
        // EEPROM-ba mentés
        if (eeprom != nullptr) {
            eeprom->saveRelayState(id, state);
        }
        return true;
    }

    bool toggleRelay(uint8_t id) {
        int idx = findIndex(id);
        if (idx < 0) return false;
        bool newState = !relayStates[idx];
        applyRelay(idx, newState);
        // EEPROM-ba mentés
        if (eeprom != nullptr) {
            eeprom->saveRelayState(id, newState);
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
        return DEVICE_CONFIG[idx].name;
    }

    // --- Skeleton: szenzor olvasás (később implementálandó) ---
    float readCurrentSensor(uint8_t id) { return 0.0f; }
    float readTempSensor(uint8_t id)    { return 0.0f; }
};

#endif // DEVICE_MANAGER_H
