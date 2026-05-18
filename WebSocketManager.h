#ifndef WEBSOCKET_MANAGER_H
#define WEBSOCKET_MANAGER_H

#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include "DeviceManager.h"
#include "WifiManager.h"
#include "NTPManager.h"

class WebSocketManager {
private:
    WebSocketsServer webSocket{81};
    DeviceManager& deviceManager;
    WifiManager& wifiManager;
    NTPManager* ntpManager = nullptr;
    EepromManager* eeprom = nullptr;

    bool hasConnectedClients = false;
    unsigned long lastWifiUpdateTime = 0;
    unsigned long lastTimeUpdateTime = 0;
    static const unsigned long WIFI_UPDATE_INTERVAL = 5000;
    static const unsigned long TIME_UPDATE_INTERVAL = 1000;

    void sendWifiStatus() {
        if (!hasConnectedClients) return;
        StaticJsonDocument<200> doc;
        doc["type"]      = "wifi";
        doc["connected"] = wifiManager.isWifiConnected();
        doc["rssi"]      = wifiManager.getRSSI();
        doc["ip"]        = wifiManager.getLocalIP();
        String json;
        serializeJson(doc, json);
        webSocket.broadcastTXT(json);
    }

    void sendTimeUpdate() {
        if (!hasConnectedClients || ntpManager == nullptr) return;
        StaticJsonDocument<128> doc;
        doc["type"]     = "time";
        doc["datetime"] = ntpManager->getDateTimeISO();
        doc["display"]  = ntpManager->getDisplayString();
        doc["synced"]   = ntpManager->isSynced();
        String json;
        serializeJson(doc, json);
        webSocket.broadcastTXT(json);
    }

    void sendRelayStatus(uint8_t id) {
        if (!hasConnectedClients) return;
        StaticJsonDocument<128> doc;
        doc["type"]  = "relay";
        doc["id"]    = id;
        doc["state"] = deviceManager.getState(id);
        doc["name"]  = deviceManager.getName(id);
        String json;
        serializeJson(doc, json);
        webSocket.broadcastTXT(json);
    }

    void sendAllStates(uint8_t clientNum) {
        sendWifiStatus();
        for (int i = 1; i <= RELAY_COUNT; i++) {
            StaticJsonDocument<128> doc;
            doc["type"]  = "relay";
            doc["id"]    = i;
            doc["state"] = deviceManager.getState(i);
            doc["name"]  = deviceManager.getName(i);
            String json;
            serializeJson(doc, json);
            webSocket.sendTXT(clientNum, json);
        }
        if (ntpManager != nullptr) {
            StaticJsonDocument<128> doc;
            doc["type"]     = "time";
            doc["datetime"] = ntpManager->getDateTimeISO();
            doc["display"]  = ntpManager->getDisplayString();
            doc["synced"]   = ntpManager->isSynced();
            String json;
            serializeJson(doc, json);
            webSocket.sendTXT(clientNum, json);
        }
    }

    void handleMessage(const String& message) {
        StaticJsonDocument<200> doc;
        DeserializationError err = deserializeJson(doc, message);
        if (err) {
            Serial.printf("[WebSocket] JSON hiba: %s\n", err.c_str());
            return;
        }

        const char* action = doc["action"];
        if (!action) return;

        if (strcmp(action, "relay") == 0) {
            uint8_t id = doc["id"];
            bool state = doc["state"];
            if (deviceManager.setRelay(id, state)) {
                sendRelayStatus(id);
            }
        }
        else if (strcmp(action, "toggle") == 0) {
            uint8_t id = doc["id"];
            if (deviceManager.toggleRelay(id)) {
                sendRelayStatus(id);
            }
        }
        else if (strcmp(action, "rename") == 0) {
            uint8_t id = doc["id"];
            const char* name = doc["name"];
            if (id >= 1 && id <= RELAY_COUNT && name) {
                if (eeprom != nullptr) {
                    eeprom->saveRelayName(id, String(name));
                }
                Serial.printf("[WebSocket] Átnevezés: Relé %d -> %s\n", id, name);
                // Broadcast az új névvel közvetlenül – ne a DeviceManager statikus nevét küldje
                StaticJsonDocument<128> renameResponse;
                renameResponse["type"]  = "relay";
                renameResponse["id"]    = id;
                renameResponse["state"] = deviceManager.getState(id);
                renameResponse["name"]  = name;
                String renameJson;
                serializeJson(renameResponse, renameJson);
                webSocket.broadcastTXT(renameJson);
            }
        }
    }

    void handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
        switch (type) {
            case WStype_DISCONNECTED:
                Serial.printf("[WebSocket] Kliens #%u lekapcsolódott\n", num);
                hasConnectedClients = webSocket.connectedClients() > 0;
                break;
            case WStype_CONNECTED:
                Serial.printf("[WebSocket] Kliens #%u kapcsolódott: %s\n",
                    num, webSocket.remoteIP(num).toString().c_str());
                hasConnectedClients = true;
                sendAllStates(num);
                break;
            case WStype_TEXT:
                Serial.printf("[WebSocket] Üzenet: %s\n", (char*)payload);
                handleMessage(String((char*)payload));
                break;
            case WStype_ERROR:
                Serial.printf("[WebSocket] Hiba kliensnél #%u\n", num);
                break;
            default:
                break;
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

    void setNTP(NTPManager& ntp) { ntpManager = &ntp; }
    void setEeprom(EepromManager& em) { eeprom = &em; }

    void begin() {
        webSocket.begin();
        Serial.println("[WebSocket] Szerver elindult a 81-es porton");
    }

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
