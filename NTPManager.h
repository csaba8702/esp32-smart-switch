#ifndef NTP_MANAGER_H
#define NTP_MANAGER_H

#include <Arduino.h>
#include <time.h>
#include <esp_sntp.h> 
#include "WifiManager.h"

#define NTP_SERVER_1        "hu.pool.ntp.org"    
#define NTP_SERVER_2        "pool.ntp.org"       
#define NTP_SERVER_3        "time.google.com"    
#define NTP_TIMEZONE        "CET-1CEST,M3.5.0,M10.5.0/3"
#define NTP_RETRY_INTERVAL  15000UL     

class NTPManager {
private:
    WifiManager& wifiManager;
    bool _synced = false; // Statikus helyett példány szintű változó
    unsigned long lastRetryTime = 0;

public:
    NTPManager(WifiManager& wm) : wifiManager(wm) {}

    void begin() {
        Serial.println("[NTP] Inicializálás...");
        _synced = false;
        // Az SNTP konfigurálása
        configTzTime(NTP_TIMEZONE, NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);
    }

    void handle() {
        // Ha már szinkronban vagyunk, nem kell semmit tenni
        if (_synced) return;

        // AP módban nincs internet
        if (wifiManager.getMode() == WifiMode::AP) return;

        unsigned long now = millis();
        if (now - lastRetryTime >= NTP_RETRY_INTERVAL) {
            lastRetryTime = now;

            if (!wifiManager.isWifiConnected()) return;

            // Az idő lekérése. Ha > 2026-os év (1.7e9 epoch), akkor a rendszer már szinkronizált
            time_t current = time(nullptr);
            if (current > 1767225600UL) {
                _synced = true;
                Serial.println("[NTP] Idő validálva. Szinkron kész.");
            } else {
                Serial.println("[NTP] Szinkronizálás folyamatban...");
            }
        }
    }

    bool isSynced() const { return _synced; }

    time_t getEpoch() const {
        return _synced ? time(nullptr) : 0;
    }

    struct tm getTime() const {
        struct tm ti;
        time_t now = time(nullptr);
        localtime_r(&now, &ti);
        return ti;
    }

    String getISOString() {
        if (!_synced) return "Nincs szinkron";
        struct tm ti = getTime();
        char buf[25];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d",
            ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
            ti.tm_hour, ti.tm_min, ti.tm_sec);
        return String(buf);
    }

    String getDisplayString() {
        if (!_synced) return "Szinkronizálás...";
        struct tm ti = getTime();
        char buf[25];
        snprintf(buf, sizeof(buf), "%04d.%02d.%02d. %02d:%02d:%02d",
            ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
            ti.tm_hour, ti.tm_min, ti.tm_sec);
        return String(buf);
    }

    String getDayOfWeek() {
        if (!_synced) return "";
        struct tm ti = getTime();
        const char* days[] = {"Vasárnap","Hétfő","Kedd","Szerda","Csütörtök","Péntek","Szombat"};
        return String(days[ti.tm_wday]);
    }
};

#endif // NTP_MANAGER_H