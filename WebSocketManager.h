#ifndef WEBSOCKET_MANAGER_H
#define WEBSOCKET_MANAGER_H

#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include "DeviceManager.h"
#include "WifiManager.h"
#include "NTPManager.h"
#include "ScheduleManager.h"

class EepromManager;

class WebSocketManager {
private:
    WebSocketsServer webSocket{81};
    DeviceManager&   deviceManager;
    WifiManager&     wifiManager;
    NTPManager*      ntpManager     = nullptr;
    EepromManager*   eeprom         = nullptr;
    ScheduleManager* scheduleManager = nullptr;

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
        String json; serializeJson(doc, json);
        webSocket.broadcastTXT(json);
    }

    void sendTimeUpdate() {
        if (!hasConnectedClients || ntpManager == nullptr) return;
        StaticJsonDocument<128> doc;
        doc["type"]     = "time";
        doc["datetime"] = ntpManager->getISOString();
        doc["display"]  = ntpManager->getDisplayString();
        String json; serializeJson(doc, json);
        webSocket.broadcastTXT(json);
    }

    // ----------------------------------------------------------------
    // full_state küldés – relék + időzítések (ONE_TIME és WEEKLY egyben)
    // ----------------------------------------------------------------
    void sendAllStates(uint8_t num) {
        StaticJsonDocument<2048> doc;
        doc["type"] = "full_state";

        // ESP32 memória adatok – minden küldésnél frissen lekérve
        doc["heap_free"]    = (uint32_t)ESP.getFreeHeap();
        doc["heap_min"]     = (uint32_t)ESP.getMinFreeHeap();
        doc["heap_total"]   = (uint32_t)ESP.getHeapSize();
        doc["psram_free"]   = (uint32_t)ESP.getFreePsram();   // 0 ha nincs PSRAM

        JsonArray relays = doc.createNestedArray("relays");
        for (uint8_t i = 1; i <= RELAY_COUNT; i++) {
            JsonObject ro = relays.createNestedObject();
            ro["id"]     = i;
            ro["name"]   = deviceManager.getName(i);
            ro["state"]  = deviceManager.getState(i);
            ro["uptime"] = deviceManager.getUptime(i, ntpManager ? (uint32_t)time(nullptr) : 0);
        }

        if (scheduleManager != nullptr) {
            JsonArray rules = doc.createNestedArray("schedules");
            for (uint8_t r = 1; r <= RELAY_COUNT; r++) {
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

        String json; serializeJson(doc, json);
        if (num == 50) webSocket.broadcastTXT(json);
        else           webSocket.sendTXT(num, json);
    }

    // ----------------------------------------------------------------
    // WebSocket üzenetek feldolgozása
    // ----------------------------------------------------------------
    void handleMessage(const char* message) {
        StaticJsonDocument<512> doc;
        if (deserializeJson(doc, message)) return;

        // const char* direkt összehasonlítás – nincs String heap allokáció
        const char* type = doc["type"] | "";

        if (strcmp(type, "toggle") == 0) {
            deviceManager.toggleRelay(doc["id"].as<uint8_t>());
            sendAllStates(50);
        }
        else if (strcmp(type, "rename") == 0) {
            deviceManager.renameRelay(doc["id"].as<uint8_t>(), doc["name"].as<String>());
            sendAllStates(50);
        }
        // ---- Egyszeri időzítés hozzáadása ----
        else if (strcmp(type, "add_schedule_once") == 0) {
            if (!scheduleManager) return;
            ScheduleRule rule{};
            rule.id        = doc["id"].as<uint32_t>();
            rule.type      = ScheduleType::ONE_TIME;
            rule.action    = (ScheduleAction)doc["action"].as<uint8_t>();
            rule.endAction = (ScheduleAction)doc["endAction"].as<uint8_t>();
            rule.validFrom = doc["from"].as<uint32_t>();
            rule.validTo   = doc["to"].as<uint32_t>();
            if (scheduleManager->addSchedule(doc["relay"].as<uint8_t>(), rule))
                sendAllStates(50);
        }
        // ---- Heti ismétlődő időzítés hozzáadása ----
        else if (strcmp(type, "add_schedule_weekly") == 0) {
            if (!scheduleManager) return;
            ScheduleRule rule{};
            rule.id        = doc["id"].as<uint32_t>();
            rule.type      = ScheduleType::WEEKLY;
            rule.action    = (ScheduleAction)doc["action"].as<uint8_t>();
            rule.endAction = (ScheduleAction)doc["endAction"].as<uint8_t>();
            rule.dayMask  = doc["dayMask"].as<uint8_t>();   // bit0=H..bit6=V
            rule.startSec = doc["startSec"].as<uint32_t>(); // másodpercek éjféltől
            rule.endSec   = doc["endSec"].as<uint32_t>();
            if (scheduleManager->addSchedule(doc["relay"].as<uint8_t>(), rule))
                sendAllStates(50);
        }
        // ---- Törlés ----
        else if (strcmp(type, "delete_schedule") == 0) {
            if (!scheduleManager) return;
            if (scheduleManager->deleteSchedule(doc["relay"].as<uint8_t>(), doc["id"].as<uint32_t>()))
                sendAllStates(50);
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
};

#endif // WEBSOCKET_MANAGER_H
