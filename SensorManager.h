#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <functional>
#include "BaseSensor.h"
#include "CurrentSensor.h"
// #include "TempSensor.h"

// ----------------------------------------------------------------
// SensorManager
//
// A szenzor–relé kapcsolat UUID alapján történik, nem relayId-val,
// mert a relayId újraindítás vagy konfiguráció változás után
// megváltozhat, az UUID viszont állandó.
//
// DeviceManager-t referencia alapján kap, hogy UUID → aktuális ID
// fordítást el tudja végezni riasztáskor.
//
// Serial mock parancsok:
//   csX:Y.YY  → X = sensorId, Y = áram (A)
//   list      → szenzor lista
// ----------------------------------------------------------------

#define MAX_SENSORS 16  // 8 relé × 2 szenzor

class DeviceManager;  // forward declaration

class SensorManager {
private:
    BaseSensor*   _sensors[MAX_SENSORS] = {nullptr};
    uint8_t       _count   = 0;
    uint8_t       _nextId  = 1;

    DeviceManager* _deviceManager = nullptr;

    // Alert callback: (relayUUID, sensorId, type, value, msg)
    std::function<void(uint32_t, uint8_t,
                       const char*, float,
                       const char*)> _onAlert = nullptr;

    // Serial puffer
    static const int CMD_BUF_SIZE = 32;
    char    _cmdBuf[CMD_BUF_SIZE];
    uint8_t _cmdIdx = 0;

    // ---- Serial parancs feldolgozás ----
    void processCommand(const char* cmd) {
        Serial.printf("[SensorManager] Parancs: %s\n", cmd);

        // CurrentSensor mock: "csX:Y.YY"
        if (strncmp(cmd, "cs", 2) == 0) {
            const char* colon = strchr(cmd, ':');
            if (!colon) {
                Serial.println("[SensorManager] Hiba: hiányzó ':'");
                return;
            }
            uint8_t id  = (uint8_t)atoi(cmd + 2);
            float   val = atof(colon + 1);
            bool found = false;
            for (uint8_t i = 0; i < _count; i++) {
                if (_sensors[i] && _sensors[i]->sensorId == id
                    && strcmp(_sensors[i]->getType(), "CURRENT") == 0)
                {
                    static_cast<CurrentSensor*>(_sensors[i])->injectMockValue(val);
                    found = true;
                    break;
                }
            }
            if (!found)
                Serial.printf("[SensorManager] Nincs CURRENT szenzor id=%d\n", id);
            return;
        }

        // Lista
        /*
        Ha zavaró, hogy 1-es, 2-es ID-kra kell hivatkozni, a SensorManager processCommand
        metódusát a SensorManager.h-ban kiegészítheted egy list paranccsal, ami kiírja a
        Serial monitorra a szenzorok ID-jait és a hozzájuk tartozó UUID-kat:
        */
        // Add ezt a processCommand-hoz a SensorManager.h-ban
        if (cmd == "list") {
            for (int i = 0; i < _count; i++) {
                if (_sensors[i]) {
                    Serial.printf("ID: %d | Type: %s | UUID: %06X | Active: %s\n", 
                        _sensors[i]->sensorId, 
                        _sensors[i]->getType(), 
                        _sensors[i]->relayUUID,
                        _sensors[i]->active ? "IGEN" : "NEM");
                }
            }
        }

        Serial.println("[SensorManager] Ismeretlen parancs. Hasznalat: csX:Y.YY | list");
    }

    void printList() {
        Serial.println("[SensorManager] --- Szenzor lista ---");
        for (uint8_t i = 0; i < _count; i++) {
            if (!_sensors[i]) continue;
            Serial.printf("  id=%d type=%s relayUUID=%06X ready=%s val=%.2f%s alert=%s\n",
                _sensors[i]->sensorId,
                _sensors[i]->getType(),
                _sensors[i]->relayUUID,
                _sensors[i]->isReady() ? "igen" : "nem",
                _sensors[i]->read(),
                _sensors[i]->getUnit(),
                _sensors[i]->isAlert() ? _sensors[i]->getAlertMsg() : "nincs"
            );
        }
        Serial.println("[SensorManager] --------------------");
    }

public:
    // ---- DeviceManager bekötése (UUID → ID fordításhoz) ----
    void setDeviceManager(DeviceManager& dm) { _deviceManager = &dm; }

    // ---- Alert callback ----
    void setOnAlert(std::function<void(uint32_t, uint8_t, const char*, float, const char*)> callback) {
        _onAlert = callback;
    }

    // ---- Szenzor regisztrálása UUID alapján ----
    bool registerSensor(BaseSensor* sensor, uint32_t relayUUID) {
        if (_count >= MAX_SENSORS || !sensor) return false;
        sensor->sensorId  = _nextId++;
        sensor->relayUUID = relayUUID;
        sensor->active    = true;
        sensor->begin();
        _sensors[_count++] = sensor;
        Serial.printf("[SensorManager] Regisztralva: id=%d type=%s relayUUID=%06X\n",
            sensor->sensorId, sensor->getType(), relayUUID);
        return true;
    }

    // ---- Törlés sensorId alapján ----
    bool removeSensor(uint8_t sensorId) {
        for (uint8_t i = 0; i < _count; i++) {
            if (_sensors[i] && _sensors[i]->sensorId == sensorId) {
                delete _sensors[i];
                for (uint8_t j = i; j < _count - 1; j++)
                    _sensors[j] = _sensors[j + 1];
                _sensors[--_count] = nullptr;
                Serial.printf("[SensorManager] Torolve: sensorId=%d\n", sensorId);
                return true;
            }
        }
        return false;
    }

    // ---- Cascade delete UUID alapján ----
    void deleteByRelayUUID(uint32_t relayUUID) {
        Serial.printf("[SensorManager] Cascade delete: relayUUID=%06X\n", relayUUID);
        for (int i = (int)_count - 1; i >= 0; i--) {
            if (_sensors[i] && _sensors[i]->relayUUID == relayUUID) {
                uint8_t sid = _sensors[i]->sensorId;
                delete _sensors[i];
                for (uint8_t j = i; j < _count - 1; j++)
                    _sensors[j] = _sensors[j + 1];
                _sensors[--_count] = nullptr;
                Serial.printf("[SensorManager] Torolve: sensorId=%d (UUID=%06X)\n",
                              sid, relayUUID);
            }
        }
    }

    // ---- Lekérdezők ----
    uint8_t getCount() const { return _count; }

    BaseSensor* getSensor(uint8_t sensorId) const {
        for (uint8_t i = 0; i < _count; i++)
            if (_sensors[i] && _sensors[i]->sensorId == sensorId)
                return _sensors[i];
        return nullptr;
    }

    uint8_t getSensorsByUUID(uint32_t relayUUID,
                              BaseSensor** out, uint8_t maxOut) const {
        uint8_t found = 0;
        for (uint8_t i = 0; i < _count && found < maxOut; i++)
            if (_sensors[i] && _sensors[i]->relayUUID == relayUUID)
                out[found++] = _sensors[i];
        return found;
    }

    // ---- begin() ----
    void begin() {
        Serial.println("[SensorManager] Inicializalva.");
        Serial.println("  Mock parancsok: csX:Y.YY (pl cs1:2.50) | list");
    }

    // ---- handle() ----
    void handle() {
        // Serial olvasás
        while (Serial.available()) {
            char c = Serial.read();
            if (c == '\n' || c == '\r') {
                if (_cmdIdx > 0) {
                    _cmdBuf[_cmdIdx] = '\0';
                    processCommand(_cmdBuf);
                    _cmdIdx = 0;
                }
            } else if (_cmdIdx < CMD_BUF_SIZE - 1) {
                _cmdBuf[_cmdIdx++] = c;
            }
        }

        // Polling + riasztás
        for (uint8_t i = 0; i < _count; i++) {
            if (!_sensors[i] || !_sensors[i]->active) continue;
            _sensors[i]->handle();
            if (_sensors[i]->isAlert() && _onAlert) {
                _onAlert(
                    _sensors[i]->relayUUID,
                    _sensors[i]->sensorId,
                    _sensors[i]->getType(),
                    _sensors[i]->read(),
                    _sensors[i]->getAlertMsg()
                );
            }
        }
    }

    // SensorManager.h - Add hozzá a publikus részhez

void addSensor(uint32_t relayUUID, const char* type, uint8_t pin) {
    if (_count >= MAX_SENSORS) {
        Serial.println("[SensorManager] Hiba: Túl sok szenzor!");
        return;
    }

    BaseSensor* newSensor = nullptr;
    String t = String(type);
    t.toUpperCase();

    // Objektum létrehozása típus alapján (a "Factory" minta egy egyszerű formája)
    if (t == "CURRENT") {
        newSensor = new CurrentSensor(pin); 
    } 
    // Ide jöhet majd a "TEMP" vagy "FLOW"
    // else if (t == "TEMP") { newSensor = new TempSensor(pin); }

    if (newSensor) {
        newSensor->sensorId = _nextId++;
        newSensor->relayUUID = relayUUID;
        newSensor->begin();
        
        _sensors[_count++] = newSensor;
        Serial.printf("[SensorManager] Szenzor hozzáadva: %s (ID: %d, UUID: %06X)\n", 
            type, newSensor->sensorId, relayUUID);
    }
}

    ~SensorManager() {
        for (uint8_t i = 0; i < _count; i++)
            if (_sensors[i]) delete _sensors[i];
    }
};

#endif // SENSOR_MANAGER_H
