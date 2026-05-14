#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>

class SensorManager {
private:
    static const int COMMAND_BUFFER_SIZE = 10;
    char cmdBuffer[COMMAND_BUFFER_SIZE];
    int cmdIndex = 0;

    struct EndStops {
        bool gate1Open = false;   // 1-es kapu nyitott végállás
        bool gate1Closed = false; // 1-es kapu zárt végállás
        bool gate2Open = false;   // 2-es kapu nyitott végállás
        bool gate2Closed = false; // 2-es kapu zárt végállás
    } endStops;

    void processCommand(const String& cmd) {
        // 1o - 1-es kapu nyitott végállás
        // 1c - 1-es kapu zárt végállás
        // 2o - 2-es kapu nyitott végállás
        // 2c - 2-es kapu zárt végállás
        if (cmd == "1o") {
            endStops.gate1Open = !endStops.gate1Open;
            Serial.printf("1-es kapu nyitott végállás: %s\n", endStops.gate1Open ? "AKTÍV" : "INAKTÍV");
        }
        else if (cmd == "1c") {
            endStops.gate1Closed = !endStops.gate1Closed;
            Serial.printf("1-es kapu zárt végállás: %s\n", endStops.gate1Closed ? "AKTÍV" : "INAKTÍV");
        }
        else if (cmd == "2o") {
            endStops.gate2Open = !endStops.gate2Open;
            Serial.printf("2-es kapu nyitott végállás: %s\n", endStops.gate2Open ? "AKTÍV" : "INAKTÍV");
        }
        else if (cmd == "2c") {
            endStops.gate2Closed = !endStops.gate2Closed;
            Serial.printf("2-es kapu zárt végállás: %s\n", endStops.gate2Closed ? "AKTÍV" : "INAKTÍV");
        }
    }

public:
    void begin() {
        Serial.println("SensorManager inicializálva");
        Serial.println("Használható parancsok:");
        Serial.println("1o - 1-es kapu nyitott végállás kapcsoló");
        Serial.println("1c - 1-es kapu zárt végállás kapcsoló");
        Serial.println("2o - 2-es kapu nyitott végállás kapcsoló");
        Serial.println("2c - 2-es kapu zárt végállás kapcsoló");
    }

    void handle() {
        while (Serial.available()) {
            char c = Serial.read();
            if (c == '\n' || c == '\r' || c == ' ') {
                cmdBuffer[cmdIndex] = '\0';
                if (cmdIndex > 0) {
                    processCommand(String(cmdBuffer));
                }
                cmdIndex = 0;
            }
            else if (cmdIndex < COMMAND_BUFFER_SIZE - 1) {
                cmdBuffer[cmdIndex++] = c;
            }
        }
    }

    // Végállás lekérdező függvények
    bool isGate1OpenEndStop() const { return endStops.gate1Open; }
    bool isGate1ClosedEndStop() const { return endStops.gate1Closed; }
    bool isGate2OpenEndStop() const { return endStops.gate2Open; }
    bool isGate2ClosedEndStop() const { return endStops.gate2Closed; }

    // Végállás kapcsolók törlése
    void clearGate1OpenEndStop() { endStops.gate1Open = false; }
    void clearGate1ClosedEndStop() { endStops.gate1Closed = false; }
    void clearGate2OpenEndStop() { endStops.gate2Open = false; }
    void clearGate2ClosedEndStop() { endStops.gate2Closed = false; }
};

#endif 