#ifndef GATE_CONTROLLER_H
#define GATE_CONTROLLER_H

#include <ArduinoJson.h>
#include <functional>
#include "SensorManager.h"

class GateController {
private:
    // Alapvető kapu állapotok
    enum class GateState {
        CLOSED,
        OPENING,
        OPENED,
        CLOSING,
        PAUSED
    };

    struct Gate {
        GateState state = GateState::CLOSED;
        bool isMoving = false;
        int position = 0;  // 0-100% között
    };

    Gate gates[2];  // Két kapu kezelése (0: bal, 1: jobb)
    SensorManager& sensorManager;  // Referencia a SensorManager-re
    std::function<void(int gateNumber, const String& statusJson)> onStateChangeCallback = nullptr;
    static const unsigned long MOTOR_MOVE_TIME = 2000;  // 2 másodperc
    unsigned long lastPrintTime = 0;  // Utolsó kiírás ideje
    static const unsigned long PRINT_INTERVAL = 2000;  // 2 másodperc

    // Állapot JSON-be konvertálása WebSocket kommunikációhoz
    String gateToJson(int gateNumber, const Gate& gate) {
        StaticJsonDocument<200> doc;
        doc["gate"] = gateNumber;
        doc["state"] = static_cast<int>(gate.state);
        doc["isMoving"] = gate.isMoving;
        doc["position"] = gate.position;

        String output;
        serializeJson(doc, output);
        return output;
    }

    void notifyStateChange(int gateNumber) {
        StaticJsonDocument<200> doc;
        doc["type"] = "gate";
        doc["gate"] = gateNumber;
        doc["state"] = static_cast<int>(gates[gateNumber-1].state);
        doc["isMoving"] = gates[gateNumber-1].isMoving;
        doc["position"] = gates[gateNumber-1].position;

        if (onStateChangeCallback) {
            String jsonString;
            serializeJson(doc, jsonString);
            onStateChangeCallback(gateNumber, jsonString);
        }
    }

public:
    GateController(SensorManager& sm) : sensorManager(sm) {
        gates[0] = Gate();
        gates[1] = Gate();
    }

    void moveGate(int gateNumber) {
        if (gateNumber < 1 || gateNumber > 2) return;
        
        int index = gateNumber - 1;
        Gate& gate = gates[index];

        // Biztonsági ellenőrzés: ha már nyitott végállásban van, jelezzük és küldjünk állapotfrissítést
        if ((gateNumber == 1 && sensorManager.isGate1OpenEndStop()) ||
            (gateNumber == 2 && sensorManager.isGate2OpenEndStop())) {
            Serial.printf("Motor %d már nyitott végállásban van - motor védelem aktiválva\n", gateNumber);
            // Küldjünk egy állapotfrissítést isMoving=false értékkel
            gate.isMoving = false;
            notifyStateChange(gateNumber);
            return;
        }

        // Ha zárt végállásban van, automatikusan inaktiváljuk azt a nyitás előtt
        if (gateNumber == 1 && sensorManager.isGate1ClosedEndStop()) {
            sensorManager.clearGate1ClosedEndStop();
            Serial.printf("Motor %d zárt végállás inaktiválva, nyitás indítható\n", gateNumber);
        } else if (gateNumber == 2 && sensorManager.isGate2ClosedEndStop()) {
            sensorManager.clearGate2ClosedEndStop();
            Serial.printf("Motor %d zárt végállás inaktiválva, nyitás indítható\n", gateNumber);
        }

        // Állapot változtatás
        gate.isMoving = true;
        gate.state = GateState::OPENING;
        Serial.printf("Motor %d nyit\n", gateNumber);
        
        notifyStateChange(gateNumber);
    }

    void handle() {
        unsigned long currentTime = millis();
        
        // 2 másodpercenként kiírjuk a motorok állapotát
        if (currentTime - lastPrintTime >= PRINT_INTERVAL) {
            for (int i = 0; i < 2; i++) {
                Gate& gate = gates[i];
                if (gate.isMoving) {
                    if (gate.state == GateState::OPENING) {
                        Serial.printf("Motor %d nyit\n", i + 1);
                    } else if (gate.state == GateState::CLOSING) {
                        Serial.printf("Motor %d zár\n", i + 1);
                    }
                }
            }
            lastPrintTime = currentTime;
        }

        // Végállás kapcsolók ellenőrzése
        updateGateState(1, sensorManager.isGate1OpenEndStop(), sensorManager.isGate1ClosedEndStop());
        updateGateState(2, sensorManager.isGate2OpenEndStop(), sensorManager.isGate2ClosedEndStop());
    }

    // Callback beállítása állapotváltozáshoz
    void setOnStateChange(std::function<void(int gateNumber, const String& statusJson)> callback) {
        onStateChangeCallback = callback;
    }

    // Kapu állapotának lekérdezése
    const Gate& getGateState(int gateNumber) const {
        return gates[gateNumber - 1];
    }

    // Kapu állapotának lekérése JSON formátumban
    String getGateStateJson(int gateNumber) {
        if (gateNumber < 1 || gateNumber > 2) return "{}";
        return gateToJson(gateNumber, gates[gateNumber - 1]);
    }

    void closeGate(int gateNumber) {
        if (gateNumber < 1 || gateNumber > 2) return;
        
        int index = gateNumber - 1;
        Gate& gate = gates[index];

        // Biztonsági ellenőrzés: ha már zárt végállásban van, jelezzük és küldjünk állapotfrissítést
        if ((gateNumber == 1 && sensorManager.isGate1ClosedEndStop()) ||
            (gateNumber == 2 && sensorManager.isGate2ClosedEndStop())) {
            Serial.printf("Motor %d már zárt végállásban van - motor védelem aktiválva\n", gateNumber);
            // Küldjünk egy állapotfrissítést isMoving=false értékkel
            gate.isMoving = false;
            notifyStateChange(gateNumber);
            return;
        }

        // Ha nyitott végállásban van, automatikusan inaktiváljuk azt a zárás előtt
        if (gateNumber == 1 && sensorManager.isGate1OpenEndStop()) {
            sensorManager.clearGate1OpenEndStop();
            Serial.printf("Motor %d nyitott végállás inaktiválva, zárás indítható\n", gateNumber);
        } else if (gateNumber == 2 && sensorManager.isGate2OpenEndStop()) {
            sensorManager.clearGate2OpenEndStop();
            Serial.printf("Motor %d nyitott végállás inaktiválva, zárás indítható\n", gateNumber);
        }

        // Állapot változtatás - csak akkor, ha tényleg elindulhat a motor
        gate.isMoving = true;
        gate.state = GateState::CLOSING;
        Serial.printf("Motor %d zár\n", gateNumber);
        
        notifyStateChange(gateNumber);
    }

    void updateGateState(int gateNumber, bool isOpenEndStop, bool isClosedEndStop) {
        if (gateNumber < 1 || gateNumber > 2) return;
        
        int index = gateNumber - 1;
        Gate& gate = gates[index];

        static bool lastOpenState[2] = {false, false};
        static bool lastClosedState[2] = {false, false};
        static bool lastMovingState[2] = {false, false};
        
        bool stateChanged = lastOpenState[index] != isOpenEndStop || 
                           lastClosedState[index] != isClosedEndStop ||
                           lastMovingState[index] != gate.isMoving;
        
        if (stateChanged) {
            // Csak akkor állítjuk meg a kaput, ha a megfelelő végálláskapcsoló aktiválódik
            if (isOpenEndStop && gate.isMoving && gate.state == GateState::OPENING) {
                gate.isMoving = false;
                gate.state = GateState::OPENED;
                gate.position = 100;
                Serial.printf("Motor %d megállt (nyitott végállás)\n", gateNumber);
                notifyStateChange(gateNumber);
            }
            else if (isClosedEndStop && gate.isMoving && gate.state == GateState::CLOSING) {
                gate.isMoving = false;
                gate.state = GateState::CLOSED;
                gate.position = 0;
                Serial.printf("Motor %d megállt (zárt végállás)\n", gateNumber);
                notifyStateChange(gateNumber);
            }
            
            lastOpenState[index] = isOpenEndStop;
            lastClosedState[index] = isClosedEndStop;
            lastMovingState[index] = gate.isMoving;
        }
    }
};

#endif