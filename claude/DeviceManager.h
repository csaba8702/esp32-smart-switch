#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <Arduino.h>
#include <time.h>
#include "DeviceTypes.h"
#include "EepromManager.h"

// Maximum relé szám – fix tömb méret, de aktív relék száma
// EEPROM-ból töltődik be, alapértelmezetten 0
#define RELAY_COUNT MAX_RELAY_COUNT  // = 4, az EepromManager-ből

// ---------------------------------------------------------------
// A két statikus tömb helyett most tagváltozók – EEPROM-ból töltve
// Alapesetben egy relé sincs definiálva (activeRelayCount = 0)
// ---------------------------------------------------------------

class DeviceManager {
private:
    DeviceConfig   configs[RELAY_COUNT];
    char           nameBuffers[RELAY_COUNT][32];
    bool           relayStates[RELAY_COUNT]     = {false};
    uint32_t       relayStartTimes[RELAY_COUNT] = {0};
    uint8_t        activeRelayCount             = 0;
    EepromManager* eeprom                       = nullptr;

    // Index keresés ID alapján – csak aktív reléknél
    int findIndex(uint8_t id) const {
        for (int i = 0; i < activeRelayCount; i++) {
            if (configs[i].id == id) return i;
        }
        return -1;
    }

public:
    DeviceManager() {
        // Tömbök nullázása
        memset(configs,     0, sizeof(configs));
        memset(nameBuffers, 0, sizeof(nameBuffers));
    }

    void setEeprom(EepromManager& em) { eeprom = &em; }

    // Aktív relék száma (0 ha még nincs konfigurálva)
    uint8_t getRelayCount() const { return activeRelayCount; }

    void begin() {
        if (eeprom == nullptr) {
            Serial.println("[Device] Nincs EEPROM – nulla rele aktiv.");
            return;
        }

        activeRelayCount = 0;

        for (uint8_t i = 0; i < RELAY_COUNT; i++) {
            bool    active;
            uint8_t pin, devType, modType;
            bool    activeLow;

            eeprom->loadRelayConfig(i, active, pin, activeLow, devType, modType);

            if (!active || pin == 0) {
                // Ez a slot üres – nem töltjük be
                continue;
            }

            // Config feltöltése
            uint8_t id = i + 1;
            configs[activeRelayCount].id           = id;
            configs[activeRelayCount].relayPin      = pin;
            configs[activeRelayCount].relayActiveLOW = activeLow;
            configs[activeRelayCount].type          = (DeviceType)devType;
            configs[activeRelayCount].module        = (ModuleType)modType;

            // Név betöltése EEPROM-ból
            String savedName = eeprom->loadRelayName(id);
            if (savedName.length() > 0) {
                strncpy(nameBuffers[activeRelayCount], savedName.c_str(), 31);
            } else {
                // Fallback: "Rele N"
                snprintf(nameBuffers[activeRelayCount], 32, "Rele %d", id);
            }
            nameBuffers[activeRelayCount][31] = '\0';
            configs[activeRelayCount].name = nameBuffers[activeRelayCount];

            // GPIO inicializálás
            pinMode(pin, OUTPUT);

            // Állapot visszaállítása
            bool savedState = eeprom->loadRelayState(id);
            relayStates[activeRelayCount] = savedState;
            bool pinLevel = activeLow ? savedState : !savedState;
            digitalWrite(pin, pinLevel);

            // Futási idő visszaállítása
            relayStartTimes[activeRelayCount] = eeprom->loadRelayStartTime(id);

            Serial.printf("[Device] Rele %d betoltve: pin=%d, nev=%s\n",
                          id, pin, nameBuffers[activeRelayCount]);
            activeRelayCount++;
        }

        Serial.printf("[Device] %d aktiv rele inicializalva.\n", activeRelayCount);
    }

    bool setRelay(uint8_t id, bool state) {
        int idx = findIndex(id);
        if (idx < 0) return false;

        relayStates[idx] = state;
        bool pinLevel = configs[idx].relayActiveLOW ? state : !state;
        digitalWrite(configs[idx].relayPin, pinLevel);

        if (eeprom != nullptr) eeprom->saveRelayState(id, state);

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
        strncpy(nameBuffers[idx], name.c_str(), 31);
        nameBuffers[idx][31] = '\0';
        configs[idx].name = nameBuffers[idx];
        if (eeprom != nullptr) eeprom->saveRelayName(id, name);
        return true;
    }

    bool getState(uint8_t id) const {
        int idx = findIndex(id);
        if (idx < 0) return false;
        return relayStates[idx];
    }

    const char* getName(uint8_t id) const {
        int idx = findIndex(id);
        if (idx < 0) return "Ismeretlen";
        return nameBuffers[idx];
    }

    uint8_t getIdByIndex(uint8_t idx) const {
        if (idx >= activeRelayCount) return 0;
        return configs[idx].id;
    }

    uint8_t getPin(uint8_t id) const {
        int idx = findIndex(id);
        if (idx < 0) return 0;
        return configs[idx].relayPin;
    }

    bool getActiveLow(uint8_t id) const {
        int idx = findIndex(id);
        if (idx < 0) return true;
        return configs[idx].relayActiveLOW;
    }

    uint32_t getUptime(uint8_t id, uint32_t currentEpoch) const {
        int idx = findIndex(id);
        if (idx < 0 || !relayStates[idx] || relayStartTimes[idx] == 0) return 0;
        if (currentEpoch < relayStartTimes[idx]) return 0;
        return currentEpoch - relayStartTimes[idx];
    }
};

#endif // DEVICE_MANAGER_H
