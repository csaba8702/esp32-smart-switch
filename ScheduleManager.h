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
    KEEP     = 2    // Lejáratkor ne csináljon semmit
};

struct ScheduleRule {
    uint32_t       id;
    ScheduleType   type;
    ScheduleAction action;      // Induló művelet
    ScheduleAction endAction;   // Lejárati művelet (TURN_OFF / TURN_ON / KEEP)

    // ONE_TIME
    uint32_t validFrom;
    uint32_t validTo;
    bool triggeredStart;
    bool triggeredEnd;

    // WEEKLY
    uint8_t  dayMask;
    uint16_t startMin;
    uint16_t endMin;
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
        if (action == ScheduleAction::KEEP) return; // Ne csináljon semmit
        bool target = (action == ScheduleAction::TURN_ON);
        if (deviceManager->getState(relayId) != target) {
            deviceManager->setRelay(relayId, target);
            Serial.printf("[Schedule] Rele %d -> %s\n", relayId, target ? "BE" : "KI");
        }
    }

    ScheduleAction inverseAction(ScheduleAction a) {
        return (a == ScheduleAction::TURN_ON) ? ScheduleAction::TURN_OFF : ScheduleAction::TURN_ON;
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
            uint32_t id, validFrom;
            uint8_t  type, dayMask, action, endAction;
            uint16_t startMin, endMin;
            bool     active;

            eeprom->loadRule(r + 1, s, id, type, dayMask, startMin, endMin, validFrom, action, endAction, active);

            if (id == 0 || id == 0xFFFFFFFF || !active) continue;

            ScheduleRule& rule = schedules[r][scheduleCounts[r]];
            rule.id        = id;
            rule.type      = (ScheduleType)type;
            rule.action    = (ScheduleAction)action;
            rule.endAction = (ScheduleAction)endAction;
            rule.isActive  = true;
            rule.dayMask   = dayMask;
            rule.startMin  = startMin;
            rule.endMin    = endMin;
            rule.validFrom = validFrom;

            if (rule.type == ScheduleType::ONE_TIME) {
                rule.validTo  = ((uint32_t)startMin << 16) | endMin;
                rule.startMin = 0;
                rule.endMin   = 0;
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
        uint32_t validFrom;
        uint16_t sMin, eMin;

        if (newRule.type == ScheduleType::ONE_TIME) {
            validFrom = newRule.validFrom;
            sMin      = (uint16_t)(newRule.validTo >> 16);
            eMin      = (uint16_t)(newRule.validTo & 0xFFFF);
            dayMask   = 0x00;
        } else {
            validFrom = 0;
            sMin      = newRule.startMin;
            eMin      = newRule.endMin;
        }
        eeprom->saveRule(relayId, slot, newRule.id, type, dayMask, sMin, eMin, validFrom, action, endAction, true);
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
                uint32_t validFrom;
                uint16_t sMin, eMin;
                if (r.type == ScheduleType::ONE_TIME) {
                    validFrom = r.validFrom;
                    sMin      = (uint16_t)(r.validTo >> 16);
                    eMin      = (uint16_t)(r.validTo & 0xFFFF);
                    dayMask   = 0x00;
                } else {
                    validFrom = 0;
                    sMin      = r.startMin;
                    eMin      = r.endMin;
                }
                eeprom->saveRule(relayId, j, r.id, type, dayMask, sMin, eMin, validFrom, action, endAction, true);
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
    uint16_t currentMin = now.tm_hour * 60 + now.tm_min;
    int      todayWday  = now.tm_wday;

    for (uint8_t r = 0; r < RELAY_COUNT; r++) {
        uint8_t  physId = r + 1;
        uint32_t toDelete[MAX_SCHEDULES_PER_RELAY];
        uint8_t  delCount = 0;

        for (uint8_t s = 0; s < scheduleCounts[r]; s++) {
            ScheduleRule& rule = schedules[r][s];
            if (!rule.isActive) continue;

            if (rule.type == ScheduleType::ONE_TIME) {
                if (currentEpoch >= rule.validFrom && currentEpoch < rule.validTo && !rule.triggeredStart) {
                    rule.triggeredStart = true;
                    executeAction(physId, rule.action);
                    anyChange = true;
                }
                if (currentEpoch >= rule.validTo && !rule.triggeredEnd) {
                    rule.triggeredEnd = true;
                    // endAction alapján dönt – KEEP esetén nem kapcsol
                    ScheduleAction endAct = rule.endAction;
                    if (endAct == ScheduleAction::KEEP) {
                        Serial.printf("[Schedule] Rele %d – lejart, allapot marad\n", physId);
                    } else {
                        executeAction(physId, endAct);
                    }
                    anyChange = true;
                    toDelete[delCount++] = rule.id;
                }
            }
            else if (rule.type == ScheduleType::WEEKLY) {
                if (rule.lastTriggeredDay != (uint8_t)todayWday) {
                    rule.todayTriggeredStart = false;
                    rule.todayTriggeredEnd   = false;
                    rule.lastTriggeredDay    = (uint8_t)todayWday;
                }

                bool dayOk = isDayActive(rule.dayMask, todayWday);
                if (!dayOk) continue;

                bool overnight = (rule.endMin < rule.startMin);
                bool inWindow  = overnight
                    ? (currentMin >= rule.startMin || currentMin < rule.endMin)
                    : (currentMin >= rule.startMin && currentMin < rule.endMin);

                if (inWindow && !rule.todayTriggeredStart) {
                    rule.todayTriggeredStart = true;
                    rule.todayTriggeredEnd   = false;
                    executeAction(physId, rule.action);
                    anyChange = true;
                }
                if (!inWindow && rule.todayTriggeredStart && !rule.todayTriggeredEnd) {
                    rule.todayTriggeredEnd = true;
                    // WEEKLY endAction: KEEP esetén marad az állapot
                    ScheduleAction endAct = rule.endAction;
                    if (endAct == ScheduleAction::KEEP) {
                        Serial.printf("[Schedule] Rele %d – heti lejart, allapot marad\n", physId);
                    } else {
                        executeAction(physId, endAct);
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
