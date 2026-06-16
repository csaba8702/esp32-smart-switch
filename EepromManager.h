#ifndef EEPROM_MANAGER_H
#define EEPROM_MANAGER_H

#include <EEPROM.h>
#include <Arduino.h>

// ---------------------------------------------------------------
// EEPROM layout – 8 reléra tervezve, ütközésmentes
//
//  [0]          Magic byte
//  [1..33]      WiFi SSID           (33 byte)
//  [34..97]     WiFi jelszó         (64 byte)
//  [98..129]    Web jelszó          (32 byte)
//  [130..162]   Session token       (33 byte)
//  [163..170]   Relé állapotok      (8×1 byte)
//  [171..202]   Relé start time-ok  (8×4 byte)
//  [203..458]   Relé nevek          (8×32 byte)
//  [459..794]   Időzítési szabályok (4 relé × 4 rule × 21 byte)
//  [795..858]   Relé konfig         (8×8 byte)
//  TOTAL: 859 < 1024 ✓
// ---------------------------------------------------------------

#define EEPROM_SIZE              1024
#define MAX_RELAY_COUNT          8
#define EEPROM_MAGIC_ADDR        0
#define EEPROM_MAGIC_VAL         0xB0   // 0xB0: layout újratervezve 8 reléra

#define EEPROM_SSID_ADDR         1
#define EEPROM_SSID_LEN          33
#define EEPROM_PASS_ADDR         34
#define EEPROM_PASS_LEN          64

#define EEPROM_WEB_PASS          98
#define EEPROM_TOKEN_ADDR        130
#define EEPROM_TOKEN_LEN         33

#define EEPROM_RELAY_STATE       163    // 8 byte (1/relé)
#define EEPROM_RELAY_START_TIME  171    // 32 byte (4/relé)
#define EEPROM_RELAY_NAME        203    // 256 byte (32/relé)

#define EEPROM_SCHEDULES_START   459
#define MAX_RULES_PER_RELAY      4
#define EEPROM_RULE_SIZE         21     // byte/rule

#define EEPROM_RELAY_CONFIG_START  795
#define EEPROM_RELAY_CONFIG_SIZE   8    // byte/relé

// Visszafelé kompatibilitás
#define RELAY_COUNT MAX_RELAY_COUNT

// ---------------------------------------------------------------
// Rule bináris layout – 21 byte / rule
//  [0..3]   id         uint32
//  [4]      type       uint8    0=ONE_TIME, 1=WEEKLY
//  [5]      dayMask    uint8
//  [6..9]   startSec   uint32
//  [10..13] endSec     uint32
//  [14..17] validFrom  uint32
//  [18]     action     uint8    0=OFF, 1=ON
//  [19]     endAction  uint8    0=OFF, 1=ON, 2=KEEP
//  [20]     active     uint8
// ---------------------------------------------------------------

class EepromManager {
private:
    bool     _commitPending = false;          // van-e nem mentett változás
    uint32_t _lastChangeMs  = 0;              // mikor volt az utolsó változás
    static const uint32_t COMMIT_DELAY_MS = 3000; // 3 mp után commitol

    void writeString(int offset, const String& str, int maxLen) {
        int len = str.length();
        if (len > maxLen - 1) len = maxLen - 1;
        for (int i = 0; i < len; i++) EEPROM.write(offset + i, str[i]);
        EEPROM.write(offset + len, '\0');
    }

    String readString(int offset, int maxLen) {
        String res = "";
        for (int i = 0; i < maxLen; i++) {
            char c = EEPROM.read(offset + i);
            if (c == '\0' || (uint8_t)c == 0xFF) break;
            res += c;
        }
        res.trim();
        return res;
    }

    void writeUint32(int offset, uint32_t val) {
        EEPROM.write(offset,     (uint8_t)(val & 0xFF));
        EEPROM.write(offset + 1, (uint8_t)((val >> 8) & 0xFF));
        EEPROM.write(offset + 2, (uint8_t)((val >> 16) & 0xFF));
        EEPROM.write(offset + 3, (uint8_t)((val >> 24) & 0xFF));
    }

    // Késleltetett commit – nem blokkol azonnal
    void markDirty() {
        _commitPending = true;
        _lastChangeMs  = millis();
    }

    uint32_t readUint32(int offset) {
        uint32_t val = 0;
        val |= (uint32_t)EEPROM.read(offset);
        val |= (uint32_t)EEPROM.read(offset + 1) << 8;
        val |= (uint32_t)EEPROM.read(offset + 2) << 16;
        val |= (uint32_t)EEPROM.read(offset + 3) << 24;
        return val;
    }

    int ruleBase(uint8_t relayId, uint8_t ruleIdx) {
        return EEPROM_SCHEDULES_START
             + (((relayId - 1) * MAX_RULES_PER_RELAY) + ruleIdx) * EEPROM_RULE_SIZE;
    }

public:
    void begin() {
        EEPROM.begin(EEPROM_SIZE);
        if (EEPROM.read(EEPROM_MAGIC_ADDR) != EEPROM_MAGIC_VAL) {
            clear();
        }
    }

    void clear() {
        for (int i = 0; i < EEPROM_SIZE; i++) EEPROM.write(i, 0);
        EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VAL);
        EEPROM.commit();
        Serial.println("[EEPROM] Memoria formazva, alaphelyzet.");
    }

    bool isValid() { return EEPROM.read(EEPROM_MAGIC_ADDR) == EEPROM_MAGIC_VAL; }

    // ---- Késleltetett commit – Main.ino loop()-ból hívni ----
    // Ha 3 másodperce nem volt változás és van pending commit, akkor ír flash-re
    void handle() {
        if (!_commitPending) return;
        if (millis() - _lastChangeMs < COMMIT_DELAY_MS) return;
        EEPROM.commit();
        _commitPending = false;
        Serial.println("[EEPROM] Commit kesz.");
    }

    // Azonnali commit – ESP.restart() előtt kötelező hívni!
    void commitNow() {
        if (_commitPending) {
            EEPROM.commit();
            _commitPending = false;
        }
    }

    // ---- WiFi ----
    void saveWiFi(const String& ssid, const String& pass) {
        writeString(EEPROM_SSID_ADDR, ssid, EEPROM_SSID_LEN);
        writeString(EEPROM_PASS_ADDR, pass, EEPROM_PASS_LEN);
        markDirty();
    }
    String loadSSID() { return readString(EEPROM_SSID_ADDR, EEPROM_SSID_LEN); }
    String loadPass() { return readString(EEPROM_PASS_ADDR, EEPROM_PASS_LEN); }

    // ---- Web jelszó / token ----
    void saveWebPassword(const String& pass) {
        writeString(EEPROM_WEB_PASS, pass, 32);
        markDirty();
    }
    String loadWebPassword() { return readString(EEPROM_WEB_PASS, 32); }

    void saveToken(const String& token) {
        writeString(EEPROM_TOKEN_ADDR, token, EEPROM_TOKEN_LEN);
        markDirty();
    }
    String loadToken() { return readString(EEPROM_TOKEN_ADDR, EEPROM_TOKEN_LEN); }

    // ---- Relé állapot (id: 1-based) ----
    void saveRelayState(uint8_t id, bool state) {
        if (id < 1 || id > MAX_RELAY_COUNT) return;
        EEPROM.write(EEPROM_RELAY_STATE + (id - 1), state ? 1 : 0);
        markDirty();
    }
    bool loadRelayState(uint8_t id) {
        if (id < 1 || id > MAX_RELAY_COUNT) return false;
        return EEPROM.read(EEPROM_RELAY_STATE + (id - 1)) == 1;
    }

    // ---- Relé start idő (id: 1-based) ----
    void saveRelayStartTime(uint8_t id, uint32_t timestamp) {
        if (id < 1 || id > MAX_RELAY_COUNT) return;
        writeUint32(EEPROM_RELAY_START_TIME + (id - 1) * 4, timestamp);
        markDirty();
    }
    uint32_t loadRelayStartTime(uint8_t id) {
        if (id < 1 || id > MAX_RELAY_COUNT) return 0;
        return readUint32(EEPROM_RELAY_START_TIME + (id - 1) * 4);
    }

    // ---- Relé név (id: 1-based) ----
    void saveRelayName(uint8_t id, const String& name) {
        if (id < 1 || id > MAX_RELAY_COUNT) return;
        writeString(EEPROM_RELAY_NAME + (id - 1) * 32, name, 32);
        markDirty();
    }
    String loadRelayName(uint8_t id) {
        if (id < 1 || id > MAX_RELAY_COUNT) return "";
        return readString(EEPROM_RELAY_NAME + (id - 1) * 32, 32);
    }

    // ---- Időzítési szabály mentés ----
    void saveRule(uint8_t relayId, uint8_t ruleIdx,
                  uint32_t id, uint8_t type, uint8_t dayMask,
                  uint32_t startSec, uint32_t endSec, uint32_t validFrom,
                  uint8_t action, uint8_t endAction, bool active)
    {
        if (relayId < 1 || relayId > MAX_RELAY_COUNT || ruleIdx >= MAX_RULES_PER_RELAY) return;
        int b = ruleBase(relayId, ruleIdx);
        writeUint32(b,       id);
        EEPROM.write(b + 4,  type);
        EEPROM.write(b + 5,  dayMask);
        writeUint32(b + 6,   startSec);
        writeUint32(b + 10,  endSec);
        writeUint32(b + 14,  validFrom);
        EEPROM.write(b + 18, action);
        EEPROM.write(b + 19, endAction);
        EEPROM.write(b + 20, active ? 1 : 0);
        markDirty();
    }

    // ---- Időzítési szabály betöltés ----
    void loadRule(uint8_t relayId, uint8_t ruleIdx,
                  uint32_t& id, uint8_t& type, uint8_t& dayMask,
                  uint32_t& startSec, uint32_t& endSec, uint32_t& validFrom,
                  uint8_t& action, uint8_t& endAction, bool& active)
    {
        if (relayId < 1 || relayId > MAX_RELAY_COUNT || ruleIdx >= MAX_RULES_PER_RELAY) return;
        int b = ruleBase(relayId, ruleIdx);
        id        = readUint32(b);
        type      = EEPROM.read(b + 4);
        dayMask   = EEPROM.read(b + 5);
        startSec  = readUint32(b + 6);
        endSec    = readUint32(b + 10);
        validFrom = readUint32(b + 14);
        action    = EEPROM.read(b + 18);
        endAction = EEPROM.read(b + 19);
        active    = (EEPROM.read(b + 20) == 1);
    }

    void clearRule(uint8_t relayId, uint8_t ruleIdx) {
        if (relayId < 1 || relayId > MAX_RELAY_COUNT || ruleIdx >= MAX_RULES_PER_RELAY) return;
        int b = ruleBase(relayId, ruleIdx);
        for (int i = 0; i < EEPROM_RULE_SIZE; i++) EEPROM.write(b + i, 0);
        markDirty();
    }

    // ---- Relay konfiguráció mentés (idx: 0-based) ----
    // uuid = 0: megőrzi a meglévőt, vagy generál újat
    void saveRelayConfig(uint8_t idx,
                         bool active, uint8_t pin, bool activeLow,
                         uint8_t devType, uint8_t modType,
                         uint32_t uuid = 0)
    {
        if (idx >= MAX_RELAY_COUNT) return;
        int b = EEPROM_RELAY_CONFIG_START + idx * EEPROM_RELAY_CONFIG_SIZE;

        if (uuid == 0 && active) {
            uint32_t existing = loadRelayUUID(idx);
            uuid = (existing != 0) ? existing : (esp_random() & 0xFFFFFF);
            if (uuid == 0) uuid = 1;
        }

        EEPROM.write(b,     active    ? 1 : 0);
        EEPROM.write(b + 1, pin);
        EEPROM.write(b + 2, activeLow ? 1 : 0);
        EEPROM.write(b + 3, devType);
        EEPROM.write(b + 4, modType);
        EEPROM.write(b + 5, (uuid >> 16) & 0xFF);
        EEPROM.write(b + 6, (uuid >> 8)  & 0xFF);
        EEPROM.write(b + 7,  uuid        & 0xFF);
        markDirty();
    }

    // ---- Relay konfiguráció betöltés (idx: 0-based) ----
    void loadRelayConfig(uint8_t idx,
                         bool& active, uint8_t& pin, bool& activeLow,
                         uint8_t& devType, uint8_t& modType,
                         uint32_t& uuid)
    {
        if (idx >= MAX_RELAY_COUNT) { uuid = 0; return; }
        int b = EEPROM_RELAY_CONFIG_START + idx * EEPROM_RELAY_CONFIG_SIZE;
        active    = (EEPROM.read(b)     == 1);
        pin       =  EEPROM.read(b + 1);
        activeLow = (EEPROM.read(b + 2) == 1);
        devType   =  EEPROM.read(b + 3);
        modType   =  EEPROM.read(b + 4);
        uuid      = ((uint32_t)EEPROM.read(b + 5) << 16)
                  | ((uint32_t)EEPROM.read(b + 6) << 8)
                  |  (uint32_t)EEPROM.read(b + 7);
    }

    uint32_t loadRelayUUID(uint8_t idx) {
        if (idx >= MAX_RELAY_COUNT) return 0;
        int b = EEPROM_RELAY_CONFIG_START + idx * EEPROM_RELAY_CONFIG_SIZE;
        return ((uint32_t)EEPROM.read(b + 5) << 16)
             | ((uint32_t)EEPROM.read(b + 6) << 8)
             |  (uint32_t)EEPROM.read(b + 7);
    }

    void clearRelayConfig(uint8_t idx) {
        if (idx >= MAX_RELAY_COUNT) return;
        int b = EEPROM_RELAY_CONFIG_START + idx * EEPROM_RELAY_CONFIG_SIZE;
        for (int i = 0; i < EEPROM_RELAY_CONFIG_SIZE; i++) EEPROM.write(b + i, 0);
        markDirty();
    }

    void dump() {}
};

#endif // EEPROM_MANAGER_H
