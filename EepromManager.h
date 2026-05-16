#ifndef EEPROM_MANAGER_H
#define EEPROM_MANAGER_H

#include <EEPROM.h>
#include <Arduino.h>

// -----------------------------------------------
// EEPROM layout (összesen 512 byte)
// -----------------------------------------------
// [0]        magic byte (0xAB = érvényes adat)
// [1..33]    WiFi SSID     (max 32 char + null)
// [34..97]   WiFi password (max 63 char + null)
// [98]       relay[0] defaultState
// [99]       relay[1] defaultState
// [100]      relay[2] defaultState
// [101]      relay[3] defaultState
// [102..133] relay[0] name (max 31 char + null)
// [134..165] relay[1] name (max 31 char + null)
// [166..197] relay[2] name (max 31 char + null)
// [198..229] relay[3] name (max 31 char + null)
// [230..261] web jelszó   (max 31 char + null)
// [262..294] session token (max 32 char + null)
// -----------------------------------------------

#define EEPROM_SIZE         512
#define EEPROM_MAGIC        0xAB
#define EEPROM_MAGIC_ADDR   0
#define EEPROM_SSID_ADDR    1
#define EEPROM_PASS_ADDR    34
#define EEPROM_RELAY_STATE  98
#define EEPROM_RELAY_NAME   102
#define EEPROM_RELAY_COUNT  4
#define EEPROM_NAME_LEN     32
#define EEPROM_WEB_PASS     230
#define EEPROM_TOKEN_ADDR   262
#define EEPROM_TOKEN_LEN    33

class EepromManager {
private:
    bool initialized = false;

    void writeString(int addr, const String& str, int maxLen) {
        int len = min((int)str.length(), maxLen - 1);
        for (int i = 0; i < len; i++) {
            EEPROM.write(addr + i, str[i]);
        }
        EEPROM.write(addr + len, '\0');
    }

    String readString(int addr, int maxLen) {
        String result = "";
        for (int i = 0; i < maxLen - 1; i++) {
            char c = (char)EEPROM.read(addr + i);
            if (c == '\0') break;
            result += c;
        }
        return result;
    }

    bool isValid() {
        return EEPROM.read(EEPROM_MAGIC_ADDR) == EEPROM_MAGIC;
    }

    void setValid() {
        EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC);
    }

public:
    bool begin() {
        if (!EEPROM.begin(EEPROM_SIZE)) {
            Serial.println("[EEPROM] Inicializálás sikertelen!");
            return false;
        }
        initialized = true;
        if (!isValid()) {
            Serial.println("[EEPROM] Nincs érvényes adat, alapértékek írása...");
            writeDefaults();
        } else {
            Serial.println("[EEPROM] Érvényes adat betöltve.");
        }
        return true;
    }

    void writeDefaults() {
        setValid();
        writeString(EEPROM_SSID_ADDR, "", 32);
        writeString(EEPROM_PASS_ADDR, "", 64);
        for (int i = 0; i < EEPROM_RELAY_COUNT; i++) {
            EEPROM.write(EEPROM_RELAY_STATE + i, 0);
            writeString(EEPROM_RELAY_NAME + i * EEPROM_NAME_LEN,
                        "Relé " + String(i + 1), EEPROM_NAME_LEN);
        }
        writeString(EEPROM_WEB_PASS, "admin", 32);
        writeString(EEPROM_TOKEN_ADDR, "", EEPROM_TOKEN_LEN);
        EEPROM.commit();
        Serial.println("[EEPROM] Alapértékek elmentve.");
    }

    void clear() {
        for (int i = 0; i < EEPROM_SIZE; i++) {
            EEPROM.write(i, 0xFF);
        }
        EEPROM.commit();
        initialized = false;
        Serial.println("[EEPROM] Törölve.");
    }

    // --- WiFi ---
    void saveWifi(const String& ssid, const String& password) {
        writeString(EEPROM_SSID_ADDR, ssid, 32);
        writeString(EEPROM_PASS_ADDR, password, 64);
        setValid();
        EEPROM.commit();
        Serial.println("[EEPROM] WiFi elmentve.");
    }

    String loadSSID()     { return readString(EEPROM_SSID_ADDR, 32); }
    String loadPassword() { return readString(EEPROM_PASS_ADDR, 64); }

    bool hasWifiCredentials() {
        return loadSSID().length() > 0;
    }

    // --- Relé állapot ---
    void saveRelayState(uint8_t id, bool state) {
        if (id < 1 || id > EEPROM_RELAY_COUNT) return;
        EEPROM.write(EEPROM_RELAY_STATE + (id - 1), state ? 1 : 0);
        EEPROM.commit();
    }

    bool loadRelayState(uint8_t id) {
        if (id < 1 || id > EEPROM_RELAY_COUNT) return false;
        return EEPROM.read(EEPROM_RELAY_STATE + (id - 1)) == 1;
    }

    // --- Relé név ---
    void saveRelayName(uint8_t id, const String& name) {
        if (id < 1 || id > EEPROM_RELAY_COUNT) return;
        writeString(EEPROM_RELAY_NAME + (id - 1) * EEPROM_NAME_LEN,
                    name, EEPROM_NAME_LEN);
        EEPROM.commit();
    }

    String loadRelayName(uint8_t id) {
        if (id < 1 || id > EEPROM_RELAY_COUNT) return "Relé " + String(id);
        return readString(EEPROM_RELAY_NAME + (id - 1) * EEPROM_NAME_LEN,
                          EEPROM_NAME_LEN);
    }

    // --- Web jelszó ---
    void saveWebPassword(const String& pass) {
        writeString(EEPROM_WEB_PASS, pass, 32);
        EEPROM.commit();
        Serial.println("[EEPROM] Web jelszó elmentve.");
    }

    String loadWebPassword() {
        return readString(EEPROM_WEB_PASS, 32);
    }

    // --- Session token ---
    void saveToken(const String& token) {
        writeString(EEPROM_TOKEN_ADDR, token, EEPROM_TOKEN_LEN);
        EEPROM.commit();
    }

    String loadToken() {
        return readString(EEPROM_TOKEN_ADDR, EEPROM_TOKEN_LEN);
    }

    // --- Debug ---
    void dump() {
        Serial.println("[EEPROM] === DUMP ===");
        Serial.printf("  Magic:    0x%02X (%s)\n",
            EEPROM.read(0), isValid() ? "OK" : "INVALID");
        Serial.printf("  SSID:     %s\n", loadSSID().c_str());
        Serial.printf("  Password: %s\n", loadPassword().c_str());
        Serial.printf("  WebPass:  %s\n", loadWebPassword().c_str());
        Serial.printf("  Token:    %s\n", loadToken().c_str());
        for (int i = 1; i <= EEPROM_RELAY_COUNT; i++) {
            Serial.printf("  Relé %d:   name='%s'  state=%s\n",
                i,
                loadRelayName(i).c_str(),
                loadRelayState(i) ? "BE" : "KI");
        }
        Serial.println("[EEPROM] ============");
    }
};

#endif // EEPROM_MANAGER_H
