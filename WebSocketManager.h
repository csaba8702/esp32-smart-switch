#ifndef WEBSOCKET_MANAGER_H
#define WEBSOCKET_MANAGER_H

#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include "GateController.h"
#include "WifiManager.h"

class WebSocketManager {
private:
    WebSocketsServer webSocket{81};
    GateController& gateController;
    WifiManager& wifiManager;
    bool hasConnectedClients = false;
    unsigned long lastWifiUpdateTime = 0;
    static const unsigned long WIFI_UPDATE_INTERVAL = 5000; // 5 másodperc

    void sendWifiStatus() {
        if (!hasConnectedClients) return;
        
        StaticJsonDocument<200> doc;
        doc["type"] = "wifi";
        doc["connected"] = wifiManager.isWifiConnected();
        doc["rssi"] = wifiManager.getRSSI();
        doc["ip"] = wifiManager.getLocalIP();

        String jsonString;
        serializeJson(doc, jsonString);
        webSocket.broadcastTXT(jsonString);
    }

    void sendGateStatus(int gateNumber, const String& statusJson) {
        if (!hasConnectedClients) return;
        String tempJson = statusJson; // Létrehozunk egy másolatot
        webSocket.broadcastTXT(tempJson);
    }

    void handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
        switch(type) {
            case WStype_DISCONNECTED:
                Serial.printf("[WebSocket] Kliens #%u lekapcsolódott\n", num);
                hasConnectedClients = webSocket.connectedClients() > 0;
                break;
                
            case WStype_CONNECTED:
                {
                    Serial.printf("[WebSocket] Kliens #%u kapcsolódott %s\n", num, webSocket.remoteIP(num).toString().c_str());
                    hasConnectedClients = true;
                    
                    // Kezdeti állapot küldése
                    sendInitialState(num);
                }
                break;
                
            case WStype_TEXT: {
                String message = String((char*)payload);
                Serial.printf("Received message: %s\n", message.c_str());
                
                if (message == "opengates") {
                    Serial.println("Executing moveGate for both gates");
                    gateController.moveGate(1);
                    gateController.moveGate(2);
                } 
                else if (message == "closegates") {
                    Serial.println("Executing closeGate for both gates");
                    gateController.closeGate(1);
                    gateController.closeGate(2);
                }
                else if (message == "opengate1") {
                    gateController.moveGate(1);
                } 
                else if (message == "opengate2") {
                    gateController.moveGate(2);
                } 
                else if (message == "closegate1") {
                    gateController.closeGate(1);
                } 
                else if (message == "closegate2") {
                    gateController.closeGate(2);
                }
                break;
            }
            
            case WStype_ERROR:
                Serial.printf("[WebSocket] Hiba történt kliensnél #%u\n", num);
                break;
        }
    }

    void sendInitialState(uint8_t clientNum) {
        // WiFi állapot küldése
        sendWifiStatus();

        // Kapuk állapotának küldése
        for (int i = 1; i <= 2; i++) {
            StaticJsonDocument<200> doc;
            doc["type"] = "gate";
            doc["gate"] = i;
            
            const auto& gateState = gateController.getGateState(i);
            doc["state"] = static_cast<int>(gateState.state);
            doc["isMoving"] = gateState.isMoving;
            doc["position"] = gateState.position;

            String json;
            serializeJson(doc, json);
            webSocket.sendTXT(clientNum, json);
        }
    }

public:
    WebSocketManager(GateController& gc, WifiManager& wm) : gateController(gc), wifiManager(wm) {
        webSocket.onEvent([this](uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
            this->handleWebSocketEvent(num, type, payload, length);
        });

        // Most már egyezik a callback szignatúra
        gateController.setOnStateChange([this](int gateNumber, const String& statusJson) {
            this->sendGateStatus(gateNumber, statusJson);
        });
    }
    
    void begin() {
        webSocket.begin();
        Serial.println("[WebSocket] Szerver elindult a 81-es porton");
    }
    
    void handle() {
        webSocket.loop();
        
        // WiFi státusz küldése 5 másodpercenként
        unsigned long currentTime = millis();
        if (currentTime - lastWifiUpdateTime >= WIFI_UPDATE_INTERVAL) {
            sendWifiStatus();
            lastWifiUpdateTime = currentTime;
        }
    }
};

#endif