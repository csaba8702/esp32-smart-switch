#ifndef EEPROM_MANAGER_H
#define EEPROM_MANAGER_H

#include <EEPROM.h>
#include <Arduino.h>

#define EEPROM_SIZE              1024
#define EEPROM_RELAY_COUNT       4
#define EEPROM_MAGIC_ADDR        0
#define EEPROM_MAGIC_VAL         0xAF   // 0xAF: relay config terület hozzáadva
#define EEPROM_SSID_ADDR         1
#define EEPROM_SSID_LEN          33
#define EEPROM_PASS_ADDR         34
#define EEPROM_PASS_LEN          64
#define EEPROM_RELAY_STATE       98
#define EEPROM_RELAY_NAME        102
#define EEPROM_WEB_PASS          230
#define EEPROM_TOKEN_ADDR        262
#define EEPROM_TOKEN_LEN         33
#define EEPROM_RELAY_START_TIME  295

#define EEPROM_SCHEDULES_START   350
#define MAX_RULES_PER_RELAY      4

// ---------------------------------------------------------------
// Relay config layout – 8 byte / relé
// EEPROM_RELAY_CONFIG_START = 686 (schedules vége után)
//
//  [0]  active    uint8  0=inaktív, 1=aktív
//  [1]  pin       uint8  GPIO pin száma
//  [2]  activeLow uint8  0=activeHIGH, 1=activeLOW
//  [3]  devType   uint8  DeviceType enum értéke
//  [4]  modType   uint8  ModuleType enum értéke
//  [5..7] padding
//
// 4 × 8 byte = 32 byte → 686+32 = 718 < 1024 ✓
// Relé neve: EEPROM_RELAY_NAME (102) – már megvan, változatlan
// ---------------------------------------------------------------
#define EEPROM_RELAY_CONFIG_START  686
#define EEPROM_RELAY_CONFIG_SIZE   8
#define MAX_RELAY_COUNT            4

// ---------------------------------------------------------------
// Rule bináris layout – 21 byte / rule
//
//  [0..3]   id         uint32
//  [4]      type       uint8    0=ONE_TIME, 1=WEEKLY
//  [5]      dayMask    uint8    WEEKLY: bit0=H..bit6=V; ONE_TIME: 0
//  [6..9]   startSec   uint32   WEEKLY: mp éjféltől (0-86399); ONE_TIME: 0
//  [10..13] endSec     uint32   WEEKLY: mp éjféltől (0-86399); ONE_TIME: 0
//  [14..17] validFrom  uint32   ONE_TIME: epoch start; WEEKLY: 0
//  [18]     action     uint8    0=OFF, 1=ON
//  [19]     endAction  uint8    0=OFF, 1=ON, 2=KEEP
//  [20]     active     uint8    0=inaktív, 1=aktív
//
// 4 relay × 4 rule × 21 byte = 336 byte → 350+336 = 686 < 1024 ✓
// ---------------------------------------------------------------
#define EEPROM_RULE_SIZE  21

class EepromManager {
private:
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

    uint32_t readUint32(int offset) {
        uint32_t val = 0;
        val |= (uint32_t)EEPROM.read(offset);
        val |= (uint32_t)EEPROM.read(offset + 1) << 8;
        val |= (uint32_t)EEPROM.read(offset + 2) << 16;
        val |= (uint32_t)EEPROM.read(offset + 3) << 24;
        return val;
    }

    int ruleBase(uint8_t relayId, uint8_t ruleIdx) {
        return EEPROM_SCHEDULES_START + (((relayId - 1) * MAX_RULES_PER_RELAY) + ruleIdx) * EEPROM_RULE_SIZE;
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

    // ---- WiFi ----
    void saveWiFi(const String& ssid, const String& pass) {
        writeString(EEPROM_SSID_ADDR, ssid, EEPROM_SSID_LEN);
        writeString(EEPROM_PASS_ADDR, pass, EEPROM_PASS_LEN);
        EEPROM.commit();
    }
    String loadSSID() { return readString(EEPROM_SSID_ADDR, EEPROM_SSID_LEN); }
    String loadPass() { return readString(EEPROM_PASS_ADDR, EEPROM_PASS_LEN); }

    // ---- Relé állapot ----
    void saveRelayState(uint8_t id, bool state) {
        if (id < 1 || id > EEPROM_RELAY_COUNT) return;
        EEPROM.write(EEPROM_RELAY_STATE + (id - 1), state ? 1 : 0);
        EEPROM.commit();
    }
    bool loadRelayState(uint8_t id) {
        if (id < 1 || id > EEPROM_RELAY_COUNT) return false;
        return EEPROM.read(EEPROM_RELAY_STATE + (id - 1)) == 1;
    }

    // ---- Relé név ----
    void saveRelayName(uint8_t id, const String& name) {
        if (id < 1 || id > EEPROM_RELAY_COUNT) return;
        writeString(EEPROM_RELAY_NAME + (id - 1) * 32, name, 32);
        EEPROM.commit();
    }
    String loadRelayName(uint8_t id) {
        if (id < 1 || id > EEPROM_RELAY_COUNT) return "";
        return readString(EEPROM_RELAY_NAME + (id - 1) * 32, 32);
    }

    // ---- Relé start idő ----
    void saveRelayStartTime(uint8_t id, uint32_t timestamp) {
        if (id < 1 || id > EEPROM_RELAY_COUNT) return;
        writeUint32(EEPROM_RELAY_START_TIME + (id - 1) * 4, timestamp);
        EEPROM.commit();
    }
    uint32_t loadRelayStartTime(uint8_t id) {
        if (id < 1 || id > EEPROM_RELAY_COUNT) return 0;
        return readUint32(EEPROM_RELAY_START_TIME + (id - 1) * 4);
    }

    // ---- Web jelszó / token ----
    void saveWebPassword(const String& pass) {
        writeString(EEPROM_WEB_PASS, pass, 32);
        EEPROM.commit();
    }
    String loadWebPassword() { return readString(EEPROM_WEB_PASS, 32); }

    void saveToken(const String& token) {
        writeString(EEPROM_TOKEN_ADDR, token, EEPROM_TOKEN_LEN);
        EEPROM.commit();
    }
    String loadToken() { return readString(EEPROM_TOKEN_ADDR, EEPROM_TOKEN_LEN); }

    // ---- Időzítési szabály mentés ----
    // startSec/endSec: WEEKLY esetén másodpercek éjféltől (0-86399); ONE_TIME esetén 0
    // validFrom: ONE_TIME esetén epoch start; WEEKLY esetén 0
    void saveRule(uint8_t relayId, uint8_t ruleIdx,
                  uint32_t id,
                  uint8_t  type,
                  uint8_t  dayMask,
                  uint32_t startSec,
                  uint32_t endSec,
                  uint32_t validFrom,
                  uint8_t  action,
                  uint8_t  endAction,
                  bool     active)
    {
        if (relayId < 1 || relayId > EEPROM_RELAY_COUNT || ruleIdx >= MAX_RULES_PER_RELAY) return;
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
        EEPROM.commit();
    }

    // ---- Időzítési szabály betöltés ----
    void loadRule(uint8_t relayId, uint8_t ruleIdx,
                  uint32_t& id,
                  uint8_t&  type,
                  uint8_t&  dayMask,
                  uint32_t& startSec,
                  uint32_t& endSec,
                  uint32_t& validFrom,
                  uint8_t&  action,
                  uint8_t&  endAction,
                  bool&     active)
    {
        if (relayId < 1 || relayId > EEPROM_RELAY_COUNT || ruleIdx >= MAX_RULES_PER_RELAY) return;
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
        if (relayId < 1 || relayId > EEPROM_RELAY_COUNT || ruleIdx >= MAX_RULES_PER_RELAY) return;
        int b = ruleBase(relayId, ruleIdx);
        for (int i = 0; i < EEPROM_RULE_SIZE; i++) EEPROM.write(b + i, 0);
        EEPROM.commit();
    }

    void dump() {}

    // ---- Relay konfiguráció mentés ----
    void saveRelayConfig(uint8_t idx,
                         bool    active,
                         uint8_t pin,
                         bool    activeLow,
                         uint8_t devType,
                         uint8_t modType)
    {
        if (idx >= MAX_RELAY_COUNT) return;
        int b = EEPROM_RELAY_CONFIG_START + idx * EEPROM_RELAY_CONFIG_SIZE;
        EEPROM.write(b,     active    ? 1 : 0);
        EEPROM.write(b + 1, pin);
        EEPROM.write(b + 2, activeLow ? 1 : 0);
        EEPROM.write(b + 3, devType);
        EEPROM.write(b + 4, modType);
        EEPROM.write(b + 5, 0);
        EEPROM.write(b + 6, 0);
        EEPROM.write(b + 7, 0);
        EEPROM.commit();
    }

    // ---- Relay konfiguráció betöltés ----
    void loadRelayConfig(uint8_t  idx,
                         bool&    active,
                         uint8_t& pin,
                         bool&    activeLow,
                         uint8_t& devType,
                         uint8_t& modType)
    {
        if (idx >= MAX_RELAY_COUNT) return;
        int b = EEPROM_RELAY_CONFIG_START + idx * EEPROM_RELAY_CONFIG_SIZE;
        active    = (EEPROM.read(b)     == 1);
        pin       =  EEPROM.read(b + 1);
        activeLow = (EEPROM.read(b + 2) == 1);
        devType   =  EEPROM.read(b + 3);
        modType   =  EEPROM.read(b + 4);
    }

    // ---- Relay config törlése (inaktív) ----
    void clearRelayConfig(uint8_t idx) {
        if (idx >= MAX_RELAY_COUNT) return;
        int b = EEPROM_RELAY_CONFIG_START + idx * EEPROM_RELAY_CONFIG_SIZE;
        for (int i = 0; i < EEPROM_RELAY_CONFIG_SIZE; i++) EEPROM.write(b + i, 0);
        EEPROM.commit();
    }
};

#endif // EEPROM_MANAGER_H
