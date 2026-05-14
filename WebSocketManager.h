#ifndef WEBSOCKET_MANAGER_H
#define WEBSOCKET_MANAGER_H

#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include "DeviceManager.h"
#include "WifiManager.h"

class WebSocketManager {
private:
    WebSocketsServer webSocket{81};
    DeviceManager& deviceManager;
    WifiManager& wifiManager;
    bool hasConnectedClients = false;
    unsigned long lastWifiUpdateTime = 0;
    static const unsigned long WIFI_UPDATE_INTERVAL = 5000;

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
        // WiFi állapot
        sendWifiStatus();
        // Mind a 4 relé állapota
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
            // {"action":"relay","id":1,"state":true}
            uint8_t id = doc["id"];
            bool state  = doc["state"];
            if (deviceManager.setRelay(id, state)) {
                sendRelayStatus(id);
            }
        }
        else if (strcmp(action, "toggle") == 0) {
            // {"action":"toggle","id":1}
            uint8_t id = doc["id"];
            if (deviceManager.toggleRelay(id)) {
                sendRelayStatus(id);
            }
        }
        // Skeleton: további parancsok (konfiguráció, szenzor lekérdezés, stb.)
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
    }
};

#endif // WEBSOCKET_MANAGER_H
