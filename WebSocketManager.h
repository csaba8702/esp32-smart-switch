#ifndef WEBSOCKET_MANAGER_H
#define WEBSOCKET_MANAGER_H

#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include "DeviceManager.h"
#include "WifiManager.h"
#include "NTPManager.h"
#include "ScheduleManager.h"
#include "SensorManager.h" // Ez kell ide, hogy a fordító lássa a metódusokat

class EepromManager;
extern uint32_t bootCount;  // Main.ino-ban definiálva

class WebSocketManager {
private:
    WebSocketsServer webSocket{81};
    DeviceManager&   deviceManager;
    WifiManager&     wifiManager;
    NTPManager*      ntpManager     = nullptr;
    EepromManager*   eeprom         = nullptr;
    ScheduleManager* scheduleManager = nullptr;
    SensorManager* sensorManager = nullptr; // <--- ADD EZT

    bool hasConnectedClients = false;
    unsigned long lastWifiUpdateTime = 0;
    unsigned long lastTimeUpdateTime = 0;
    static const unsigned long WIFI_UPDATE_INTERVAL = 5000;
    static const unsigned long TIME_UPDATE_INTERVAL  = 1000;

    // ----------------------------------------------------------------
    void sendWifiStatus() {
        if (!hasConnectedClients) return;
        StaticJsonDocument<200> doc;
        doc["type"]      = "wifi";
        doc["connected"] = wifiManager.isWifiConnected();
        doc["rssi"]      = wifiManager.getRSSI();
        doc["ip"]        = wifiManager.getLocalIP();
        doc["mode"]      = wifiManager.getModeString(); // "AP" vagy "STA"
        String json; serializeJson(doc, json);
        webSocket.broadcastTXT(json);
    }

    void sendTimeUpdate() {
        if (!hasConnectedClients || ntpManager == nullptr) return;
        StaticJsonDocument<192> doc;
        doc["type"]      = "time";
        doc["datetime"]  = ntpManager->getISOString();
        doc["display"]   = ntpManager->getDisplayString();
        // Memória adatok a time üzenetbe – így másodpercenként frissül
        // a kijelző anélkül, hogy külön üzenetre lenne szükség
        doc["heap_free"]  = (uint32_t)ESP.getFreeHeap();
        doc["heap_total"] = (uint32_t)ESP.getHeapSize();
        String json; serializeJson(doc, json);
        webSocket.broadcastTXT(json);
    }

    // ----------------------------------------------------------------
    // full_state küldés – relék + időzítések (ONE_TIME és WEEKLY egyben)
    // ----------------------------------------------------------------
void sendAllStates(uint8_t num) {
    // A puffer méretét 2048-ról 4096-ra növeltem, hogy a szenzor adatok biztosan elférjenek
    StaticJsonDocument<4096> doc;
    doc["type"] = "full_state";
    doc["boot_count"] = bootCount;

    // ESP32 memória adatok – minden küldésnél frissen lekérve
    doc["heap_free"]    = (uint32_t)ESP.getFreeHeap();
    doc["heap_min"]     = (uint32_t)ESP.getMinFreeHeap();
    doc["heap_total"]   = (uint32_t)ESP.getHeapSize();
    doc["psram_free"]   = (uint32_t)ESP.getFreePsram();   // 0 ha nincs PSRAM

    JsonArray relays = doc.createNestedArray("relays");
    uint8_t relayCount = deviceManager.getRelayCount();
    for (uint8_t i = 0; i < relayCount; i++) {
        uint8_t id = deviceManager.getIdByIndex(i);
        JsonObject ro = relays.createNestedObject();
        ro["id"]        = id;
        ro["uuid"]      = deviceManager.getUUID(id);
        ro["name"]      = deviceManager.getName(id);
        ro["state"]     = deviceManager.getState(id);
        ro["uptime"]    = deviceManager.getUptime(id, ntpManager ? (uint32_t)time(nullptr) : 0);
        ro["pin"]       = deviceManager.getPin(id);
        ro["activeLow"] = deviceManager.getActiveLow(id);

        // ---- ÚJ: Szenzorok hozzáadása a reléhez ----
        JsonArray sensors = ro.createNestedArray("sensors");
        if (sensorManager != nullptr) {
            // Segédtömb a találatokhoz (max 4 szenzor/relé)
            BaseSensor* foundSensors[4];
            uint32_t uuid = deviceManager.getUUIDasUint(id);
            uint8_t foundCount = sensorManager->getSensorsByUUID(uuid, foundSensors, 4);
            
            for (uint8_t j = 0; j < foundCount; j++) {
                JsonObject so = sensors.createNestedObject();
                so["type"]  = foundSensors[j]->getType();
                so["value"] = foundSensors[j]->read();
            }
        }
    }

    if (scheduleManager != nullptr) {
        JsonArray rules = doc.createNestedArray("schedules");
        for (uint8_t r = 1; r <= relayCount; r++) {
            for (uint8_t s = 0; s < scheduleManager->getCount(r); s++) {
                const ScheduleRule& rule = scheduleManager->getRule(r, s);
                JsonObject ro = rules.createNestedObject();
                ro["relay"]     = r;
                ro["id"]        = rule.id;
                ro["type"]      = (uint8_t)rule.type;
                ro["action"]    = (uint8_t)rule.action;
                ro["endAction"] = (uint8_t)rule.endAction;

                if (rule.type == ScheduleType::ONE_TIME) {
                    ro["from"] = rule.validFrom;
                    ro["to"]   = rule.validTo;
                } else {
                    ro["dayMask"]  = rule.dayMask;
                    ro["startSec"] = rule.startSec;
                    ro["endSec"]   = rule.endSec;
                }
            }
        }
    }

    String json;
    serializeJson(doc, json);
    if (num == 50) webSocket.broadcastTXT(json);
    else           webSocket.sendTXT(num, json);
}

    // ----------------------------------------------------------------
    // WebSocket üzenetek feldolgozása
    // ----------------------------------------------------------------
void handleMessage(const char* message) {
    // Puffer méret növelve 1024-re a biztonságosabb JSON feldolgozás érdekében
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, message);
    if (error) {
        Serial.printf("[WS] JSON hiba: %s\n", error.c_str());
        return;
    }

    const char* type = doc["type"] | "";

    // ---- Kapcsolás ----
    if (strcmp(type, "toggle") == 0) {
        deviceManager.toggleRelay(doc["id"].as<uint8_t>());
        sendAllStates(50);
    }
    // ---- Átnevezés ----
    else if (strcmp(type, "rename") == 0) {
        deviceManager.renameRelay(doc["id"].as<uint8_t>(), doc["name"].as<String>());
        sendAllStates(50);
    }
    // ---- Egyszeri időzítés hozzáadása ----
    else if (strcmp(type, "add_schedule_once") == 0) {
        if (!scheduleManager) return;
        ScheduleRule rule{};
        rule.id         = doc["id"].as<uint32_t>();
        rule.type       = ScheduleType::ONE_TIME;
        rule.action     = (ScheduleAction)doc["action"].as<uint8_t>();
        rule.endAction  = (ScheduleAction)doc["endAction"].as<uint8_t>();
        rule.validFrom  = doc["from"].as<uint32_t>();
        rule.validTo    = doc["to"].as<uint32_t>();
        if (scheduleManager->addSchedule(doc["relay"].as<uint8_t>(), rule))
            sendAllStates(50);
    }
    // ---- Heti ismétlődő időzítés hozzáadása ----
    else if (strcmp(type, "add_schedule_weekly") == 0) {
        if (!scheduleManager) return;
        ScheduleRule rule{};
        rule.id         = doc["id"].as<uint32_t>();
        rule.type       = ScheduleType::WEEKLY;
        rule.action     = (ScheduleAction)doc["action"].as<uint8_t>();
        rule.endAction  = (ScheduleAction)doc["endAction"].as<uint8_t>();
        rule.dayMask    = doc["dayMask"].as<uint8_t>();
        rule.startSec   = doc["startSec"].as<uint32_t>();
        rule.endSec     = doc["endSec"].as<uint32_t>();
        if (scheduleManager->addSchedule(doc["relay"].as<uint8_t>(), rule))
            sendAllStates(50);
    }
    // ---- Időzítés törlése ----
    else if (strcmp(type, "delete_schedule") == 0) {
        if (!scheduleManager) return;
        if (scheduleManager->deleteSchedule(doc["relay"].as<uint8_t>(), doc["id"].as<uint32_t>()))
            sendAllStates(50);
    }
    // ---- Szenzor hozzáadása ----
    else if (strcmp(type, "add_sensor") == 0) {
        // Formátum: {"type":"add_sensor", "uuid":"1A2B3C", "sensorType":"CURRENT", "pin":34}
        const char* uuidStr    = doc["uuid"] | "000000";
        const char* sensorType = doc["sensorType"] | "";
        uint8_t     pin        = doc["pin"] | 0;
        
        uint32_t uuid = (uint32_t)strtoul(uuidStr, nullptr, 16);
        
        // Feltételezzük, hogy van egy globális sensorManager példány vagy tagváltozó
        // Ha tagváltozó, győződj meg róla, hogy be van állítva
        if (sensorManager != nullptr) {
            sensorManager->addSensor(uuid, sensorType, pin);
            sendAllStates(50);
        }
    }
    // ---- Relay konfiguráció mentés ----
    else if (strcmp(type, "save_relay_config") == 0) {
        if (eeprom == nullptr) {
            Serial.println("[WS] save_relay_config: eeprom nullptr!");
            return;
        }

        JsonArray relays = doc["relays"].as<JsonArray>();
        if (relays.isNull()) {
            Serial.println("[WS] save_relay_config: relays tomb ures!");
            return;
        }

        Serial.printf("[WS] save_relay_config: %d rele erkezett\n", relays.size());

        bool slotUsed[MAX_RELAY_COUNT] = {false};
        uint8_t idx = 0;
        for (JsonObject r : relays) {
            if (idx >= MAX_RELAY_COUNT) break;
            slotUsed[idx] = true;

            uint8_t     pin       = r["pin"] | 0;
            bool        activeLow = r["activeLow"] | true;
            const char* name      = r["name"] | "Rele";
            uint32_t    uuid      = 0;
            const char* uuidStr   = r["uuid"] | "";
            if (strlen(uuidStr) == 6) {
                uuid = (uint32_t)strtoul(uuidStr, nullptr, 16) & 0xFFFFFF;
            }

            eeprom->saveRelayConfig(idx, true, pin, activeLow,
                (uint8_t)DeviceType::GENERIC, (uint8_t)ModuleType::NONE, uuid);
            eeprom->saveRelayName(idx + 1, String(name));

            Serial.printf("[WS] Relay %d mentve: pin=%d uuid=%s nev=%s\n",
                          idx + 1, pin, uuidStr, name);
            idx++;
        }

        for (uint8_t i = 0; i < MAX_RELAY_COUNT; i++) {
            if (!slotUsed[i]) {
                eeprom->clearRelayConfig(i);
            }
        }

        {
            StaticJsonDocument<64> ack;
            ack["type"] = "reboot";
            String ackJson;
            serializeJson(ack, ackJson);
            webSocket.broadcastTXT(ackJson);
        }
        eeprom->commitNow();  // Fontos: flush előtt restart!
        Serial.println("[WS] Relay config elmentve – ujraindul...");
        delay(300);
        ESP.restart();
    }
}

    void handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
        switch (type) {
            case WStype_CONNECTED:
                hasConnectedClients = true;
                sendAllStates(num);
                break;
            case WStype_TEXT:
                // A payload már null-terminált a könyvtárban, String másolat kerülendő
                handleMessage(reinterpret_cast<const char*>(payload));
                break;
            default: break;
        }
    }

public:
    WebSocketManager(DeviceManager& dm, WifiManager& wm)
        : deviceManager(dm), wifiManager(wm)
    {
        webSocket.onEvent([this](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
            this->handleWebSocketEvent(num, type, payload, length);
        });
    }

    void setNTP(NTPManager& ntp)         { ntpManager = &ntp; }
    void setEeprom(EepromManager& em)    { eeprom = &em; }

    void setScheduleManager(ScheduleManager& sm) {
        scheduleManager = &sm;
        scheduleManager->setOnStateChanged([this]() {
            if (hasConnectedClients) {
                Serial.println("[WS] Schedule valtozas -> full_state kuldese");
                sendAllStates(50);
            }
        });
    }

    void begin() { webSocket.begin(); }

    void handle() {
        webSocket.loop();
        unsigned long now = millis();
        if (now - lastWifiUpdateTime >= WIFI_UPDATE_INTERVAL) {
            sendWifiStatus();
            lastWifiUpdateTime = now;
        }
        if (now - lastTimeUpdateTime >= TIME_UPDATE_INTERVAL) {
            sendTimeUpdate();
            lastTimeUpdateTime = now;
        }
    }

    void setSensorManager(SensorManager& sm) {
        sensorManager = &sm;
        
        // Itt regisztráljuk a callbacket, ami a riasztásokat továbbítja a WebSocketre
        sensorManager->setOnAlert([this](uint32_t uuid, uint8_t sensorId, const char* type, float val, const char* msg) {
            if (hasConnectedClients) {
                StaticJsonDocument<256> doc;
                doc["type"] = "sensor_alert";
                doc["uuid"] = uuid;
                doc["sensorId"] = sensorId;
                doc["alertType"] = type;
                doc["value"] = val;
                doc["msg"] = msg;
                
                String json;
                serializeJson(doc, json);
                webSocket.broadcastTXT(json);
            }
        });
    }
};

#endif // WEBSOCKET_MANAGER_H
