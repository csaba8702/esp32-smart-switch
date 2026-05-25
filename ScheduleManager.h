#ifndef SCHEDULE_MANAGER_H
#define SCHEDULE_MANAGER_H

#include <Arduino.h>
#include <functional>
#include "DeviceManager.h"

class EepromManager;

#define MAX_SCHEDULES_PER_RELAY 4

enum class ScheduleType : uint8_t {
    ONE_TIME = 0,
    WEEKLY   = 1
};

enum class ScheduleAction : uint8_t {
    TURN_OFF = 0,
    TURN_ON  = 1,
    KEEP     = 2
};

struct ScheduleRule {
    uint32_t       id;
    ScheduleType   type;
    ScheduleAction action;
    ScheduleAction endAction;

    // ONE_TIME
    uint32_t validFrom;
    uint32_t validTo;
    bool triggeredStart;
    bool triggeredEnd;

    // WEEKLY – másodperc alapú (0-86399)
    uint8_t  dayMask;
    uint32_t startSec;   // másodpercek éjféltől
    uint32_t endSec;     // másodpercek éjféltől
    bool todayTriggeredStart;
    bool todayTriggeredEnd;
    uint8_t lastTriggeredDay;

    bool isActive;
};

class ScheduleManager {
private:
    ScheduleRule schedules[RELAY_COUNT][MAX_SCHEDULES_PER_RELAY];
    uint8_t scheduleCounts[RELAY_COUNT];

    DeviceManager* deviceManager = nullptr;
    EepromManager* eeprom        = nullptr;

    std::function<void()> onStateChangedCallback = nullptr;

    void executeAction(uint8_t relayId, ScheduleAction action) {
        if (action == ScheduleAction::KEEP) return;
        bool target = (action == ScheduleAction::TURN_ON);
        if (deviceManager->getState(relayId) != target) {
            deviceManager->setRelay(relayId, target);
            Serial.printf("[Schedule] Rele %d -> %s\n", relayId, target ? "BE" : "KI");
        }
    }

    bool isDayActive(uint8_t dayMask, int tmWday) {
        static const uint8_t wdayToBit[7] = {6, 0, 1, 2, 3, 4, 5};
        return (dayMask >> wdayToBit[tmWday]) & 0x01;
    }

public:
    ScheduleManager(DeviceManager* dm, EepromManager* em);

    void setOnStateChanged(std::function<void()> cb) {
        onStateChangedCallback = cb;
    }

    void begin();
    bool addSchedule(uint8_t relayId, const ScheduleRule& newRule);
    bool deleteSchedule(uint8_t relayId, uint32_t ruleId);
    void checkSchedules(uint32_t currentEpoch);

    uint8_t getCount(uint8_t relayId) const {
        return (relayId > 0 && relayId <= RELAY_COUNT) ? scheduleCounts[relayId - 1] : 0;
    }
    const ScheduleRule& getRule(uint8_t relayId, uint8_t idx) const {
        return schedules[relayId - 1][idx];
    }
};

#include "EepromManager.h"

inline ScheduleManager::ScheduleManager(DeviceManager* dm, EepromManager* em)
    : deviceManager(dm), eeprom(em)
{
    for (uint8_t r = 0; r < RELAY_COUNT; r++) {
        scheduleCounts[r] = 0;
        for (uint8_t s = 0; s < MAX_SCHEDULES_PER_RELAY; s++) {
            schedules[r][s] = {};
            schedules[r][s].id = 0;
            schedules[r][s].isActive = false;
        }
    }
}

inline void ScheduleManager::begin() {
    if (eeprom == nullptr) return;
    for (uint8_t r = 0; r < RELAY_COUNT; r++) {
        scheduleCounts[r] = 0;
        for (uint8_t s = 0; s < MAX_SCHEDULES_PER_RELAY; s++) {
            uint32_t id, validFrom, startSec, endSec;
            uint8_t  type, dayMask, action, endAction;
            bool     active;

            eeprom->loadRule(r + 1, s, id, type, dayMask, startSec, endSec, validFrom, action, endAction, active);

            if (id == 0 || id == 0xFFFFFFFF || !active) continue;

            ScheduleRule& rule = schedules[r][scheduleCounts[r]];
            rule.id        = id;
            rule.type      = (ScheduleType)type;
            rule.action    = (ScheduleAction)action;
            rule.endAction = (ScheduleAction)endAction;
            rule.isActive  = true;
            rule.dayMask   = dayMask;

            if (rule.type == ScheduleType::ONE_TIME) {
                // ONE_TIME: validFrom=epoch start, startSec/endSec = validTo high/low
                rule.validFrom = validFrom;
                rule.validTo   = ((uint32_t)startSec << 16) | (endSec & 0xFFFF);
                rule.startSec  = 0;
                rule.endSec    = 0;
            } else {
                // WEEKLY: másodperc alapú
                rule.startSec  = startSec;
                rule.endSec    = endSec;
                rule.validFrom = 0;
                rule.validTo   = 0;
            }

            rule.triggeredStart      = false;
            rule.triggeredEnd        = false;
            rule.todayTriggeredStart = false;
            rule.todayTriggeredEnd   = false;
            rule.lastTriggeredDay    = 0xFF;
            scheduleCounts[r]++;
        }
    }
    Serial.println("[Schedule] EEPROM betoltve.");
}

inline bool ScheduleManager::addSchedule(uint8_t relayId, const ScheduleRule& newRule) {
    uint8_t rIdx = relayId - 1;
    if (rIdx >= RELAY_COUNT || scheduleCounts[rIdx] >= MAX_SCHEDULES_PER_RELAY) return false;

    uint8_t slot = scheduleCounts[rIdx];
    schedules[rIdx][slot] = newRule;
    schedules[rIdx][slot].isActive             = true;
    schedules[rIdx][slot].triggeredStart       = false;
    schedules[rIdx][slot].triggeredEnd         = false;
    schedules[rIdx][slot].todayTriggeredStart  = false;
    schedules[rIdx][slot].todayTriggeredEnd    = false;
    schedules[rIdx][slot].lastTriggeredDay     = 0xFF;
    scheduleCounts[rIdx]++;

    if (eeprom != nullptr) {
        uint8_t  type      = (uint8_t)newRule.type;
        uint8_t  dayMask   = newRule.dayMask;
        uint8_t  action    = (uint8_t)newRule.action;
        uint8_t  endAction = (uint8_t)newRule.endAction;
        uint32_t validFrom, startSec, endSec;

        if (newRule.type == ScheduleType::ONE_TIME) {
            // validTo uint32-t két uint16-ra bontjuk a startSec/endSec mezőkbe
            validFrom = newRule.validFrom;
            startSec  = (newRule.validTo >> 16) & 0xFFFF;
            endSec    = newRule.validTo & 0xFFFF;
            dayMask   = 0x00;
        } else {
            validFrom = 0;
            startSec  = newRule.startSec;
            endSec    = newRule.endSec;
        }
        eeprom->saveRule(relayId, slot, newRule.id, type, dayMask, startSec, endSec, validFrom, action, endAction, true);
    }
    return true;
}

inline bool ScheduleManager::deleteSchedule(uint8_t relayId, uint32_t ruleId) {
    uint8_t rIdx = relayId - 1;
    if (rIdx >= RELAY_COUNT) return false;

    for (uint8_t s = 0; s < scheduleCounts[rIdx]; s++) {
        if (schedules[rIdx][s].id != ruleId) continue;

        for (uint8_t j = s; j < scheduleCounts[rIdx] - 1; j++) {
            schedules[rIdx][j] = schedules[rIdx][j + 1];
            if (eeprom != nullptr) {
                const ScheduleRule& r = schedules[rIdx][j];
                uint8_t  type      = (uint8_t)r.type;
                uint8_t  dayMask   = r.dayMask;
                uint8_t  action    = (uint8_t)r.action;
                uint8_t  endAction = (uint8_t)r.endAction;
                uint32_t validFrom, startSec, endSec;
                if (r.type == ScheduleType::ONE_TIME) {
                    validFrom = r.validFrom;
                    startSec  = (r.validTo >> 16) & 0xFFFF;
                    endSec    = r.validTo & 0xFFFF;
                    dayMask   = 0x00;
                } else {
                    validFrom = 0;
                    startSec  = r.startSec;
                    endSec    = r.endSec;
                }
                eeprom->saveRule(relayId, j, r.id, type, dayMask, startSec, endSec, validFrom, action, endAction, true);
            }
        }
        scheduleCounts[rIdx]--;
        schedules[rIdx][scheduleCounts[rIdx]] = {};
        schedules[rIdx][scheduleCounts[rIdx]].id = 0;
        schedules[rIdx][scheduleCounts[rIdx]].isActive = false;
        if (eeprom != nullptr) eeprom->clearRule(relayId, scheduleCounts[rIdx]);
        return true;
    }
    return false;
}

inline void ScheduleManager::checkSchedules(uint32_t currentEpoch) {
    if (deviceManager == nullptr) return;

    bool anyChange = false;

    struct tm now;
    time_t t = (time_t)currentEpoch;
    localtime_r(&t, &now);
    // Másodpercek éjféltől – helyi idő alapján
    uint32_t currentSec = now.tm_hour * 3600 + now.tm_min * 60 + now.tm_sec;
    int      todayWday  = now.tm_wday;

    for (uint8_t r = 0; r < RELAY_COUNT; r++) {
        uint8_t  physId = r + 1;
        uint32_t toDelete[MAX_SCHEDULES_PER_RELAY];
        uint8_t  delCount = 0;

        for (uint8_t s = 0; s < scheduleCounts[r]; s++) {
            ScheduleRule& rule = schedules[r][s];
            if (!rule.isActive) continue;

            // ---- ONE_TIME ----------------------------------------
            if (rule.type == ScheduleType::ONE_TIME) {
                if (currentEpoch >= rule.validFrom && currentEpoch < rule.validTo && !rule.triggeredStart) {
                    rule.triggeredStart = true;
                    executeAction(physId, rule.action);
                    anyChange = true;
                }
                if (currentEpoch >= rule.validTo && !rule.triggeredEnd) {
                    rule.triggeredEnd = true;
                    if (rule.endAction == ScheduleAction::KEEP) {
                        Serial.printf("[Schedule] Rele %d – lejart, allapot marad\n", physId);
                    } else {
                        executeAction(physId, rule.endAction);
                    }
                    anyChange = true;
                    toDelete[delCount++] = rule.id;
                }
            }

            // ---- WEEKLY – másodperc alapú -------------------------
            else if (rule.type == ScheduleType::WEEKLY) {
                // Napi trigger reset ha új nap
                if (rule.lastTriggeredDay != (uint8_t)todayWday) {
                    rule.todayTriggeredStart = false;
                    rule.todayTriggeredEnd   = false;
                    rule.lastTriggeredDay    = (uint8_t)todayWday;
                }

                bool dayOk = isDayActive(rule.dayMask, todayWday);
                if (!dayOk) continue;

                // Éjfél-átnyúlás kezelése (pl. 23:50:00 - 00:10:00)
                bool overnight = (rule.endSec < rule.startSec);
                bool inWindow  = overnight
                    ? (currentSec >= rule.startSec || currentSec < rule.endSec)
                    : (currentSec >= rule.startSec && currentSec < rule.endSec);

                if (inWindow && !rule.todayTriggeredStart) {
                    rule.todayTriggeredStart = true;
                    rule.todayTriggeredEnd   = false;
                    executeAction(physId, rule.action);
                    anyChange = true;
                }
                if (!inWindow && rule.todayTriggeredStart && !rule.todayTriggeredEnd) {
                    rule.todayTriggeredEnd = true;
                    if (rule.endAction == ScheduleAction::KEEP) {
                        Serial.printf("[Schedule] Rele %d – heti lejart, allapot marad\n", physId);
                    } else {
                        executeAction(physId, rule.endAction);
                    }
                    anyChange = true;
                }
            }
        }

        for (uint8_t d = 0; d < delCount; d++) {
            deleteSchedule(physId, toDelete[d]);
            anyChange = true;
        }
    }

    if (anyChange && onStateChangedCallback) {
        onStateChangedCallback();
    }
}

#endif // SCHEDULE_MANAGER_H
