#ifndef GATE_TYPES_H
#define GATE_TYPES_H

#include <ArduinoJson.h>

// Kapu állapot enum
enum class GateStateEnum {
    UNKNOWN,
    CLOSED,
    OPENING,
    OPENED,
    CLOSING,
    PAUSED
};

// Kapu adatai
struct GateData {
    int position = 0; // A szenzor értéke
    bool isMoving = false; // A motor állapota
};

// Rendszer állapot
struct SystemState {
    GateData leftGate;
    GateData rightGate;
    bool isPersonalGateMode = false;

    String toJson() const {
        StaticJsonDocument<512> doc;

        JsonObject left = doc.createNestedObject("leftGate");
        left["position"] = leftGate.position;
        left["isMoving"] = leftGate.isMoving;

        JsonObject right = doc.createNestedObject("rightGate");
        right["position"] = rightGate.position;
        right["isMoving"] = rightGate.isMoving;

        doc["isPersonalGateMode"] = isPersonalGateMode;

        String output;
        serializeJson(doc, output);
        return output;
    }
};

#endif