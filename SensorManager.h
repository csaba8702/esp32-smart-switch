#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>

// Skeleton - jelenleg csak Serial debug parancsokat kezel
// Később ide kerül: DS18B20, ACS712, stb.

class SensorManager {
private:
    static const int COMMAND_BUFFER_SIZE = 10;
    char cmdBuffer[COMMAND_BUFFER_SIZE];
    int cmdIndex = 0;

    void processCommand(const String& cmd) {
        // Skeleton: ide kerülnek majd a szenzor debug parancsok
        Serial.printf("[SensorManager] Parancs: %s\n", cmd.c_str());
    }

public:
    void begin() {
        Serial.println("[SensorManager] Inicializálva (skeleton)");
    }

    void handle() {
        // Serial parancs olvasás (debug célra)
        while (Serial.available()) {
            char c = Serial.read();
            if (c == '\n' || c == '\r') {
                cmdBuffer[cmdIndex] = '\0';
                if (cmdIndex > 0) {
                    processCommand(String(cmdBuffer));
                }
                cmdIndex = 0;
            } else if (cmdIndex < COMMAND_BUFFER_SIZE - 1) {
                cmdBuffer[cmdIndex++] = c;
            }
        }
    }

    // --- Skeleton: szenzor lekérdező függvények (később) ---
    float getTemperature(uint8_t sensorId) { return 0.0f; }
    float getCurrent(uint8_t sensorId)     { return 0.0f; }
    float getFlow(uint8_t sensorId)        { return 0.0f; }
};

#endif // SENSOR_MANAGER_H
