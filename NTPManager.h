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
#define NTP_RESYNC_INTERVAL 86400000UL  
#define NTP_RETRY_INTERVAL  15000UL     

class NTPManager {
private:
    WifiManager& wifiManager;
    static bool synced; 
    unsigned long lastRetryTime = 0;

    static void timeSyncCallback(struct timeval* tv) {
        Serial.println("[NTP] Sikeres időszinkronizálás történt a háttérben!");
        synced = true;
    }

public:
    NTPManager(WifiManager& wm) : wifiManager(wm) {}

    void begin() {
        Serial.println("[NTP] Inicializálás...");
        synced = false;
        sntp_set_time_sync_notification_cb(timeSyncCallback);
        configTzTime(NTP_TIMEZONE, NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);
    }

    void handle() {
        if (synced) return;

        unsigned long now = millis();
        if (now - lastRetryTime >= NTP_RETRY_INTERVAL) {
            lastRetryTime = now;

            if (!wifiManager.isWifiConnected()) return;

            time_t current = time(nullptr);
            if (current > 1000000000UL) {
                synced = true;
                Serial.println("[NTP] Idő validálva. Szinkron kész.");
                return;
            }
            Serial.println("[NTP] Idő kérése folyamatban...");
        }
    }

    bool isSynced() const { return synced; }

    time_t getEpoch() const {
        if (!synced) return 0;
        return time(nullptr);
    }

    struct tm getTime() const {
        struct tm ti;
        time_t now = time(nullptr);
        localtime_r(&now, &ti);
        return ti;
    }

    // Ezt keresi a WebSocketManager módosított sora!
    String getISOString() {
        if (!synced) return "Nincs szinkron";
        struct tm ti = getTime();
        char buf[25];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d",
            ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
            ti.tm_hour, ti.tm_min, ti.tm_sec);
        return String(buf);
    }

    String getDisplayString() {
        if (!synced) return "Szinkronizálás...";
        struct tm ti = getTime();
        char buf[25];
        snprintf(buf, sizeof(buf), "%04d.%02d.%02d. %02d:%02d:%02d",
            ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
            ti.tm_hour, ti.tm_min, ti.tm_sec);
        return String(buf);
    }

    String getDayOfWeek() {
        if (!synced) return "";
        struct tm ti = getTime();
        const char* days[] = {"Vasárnap","Hétfő","Kedd","Szerda","Csütörtök","Péntek","Szombat"};
        return String(days[ti.tm_wday]);
    }
};

bool NTPManager::synced = false;

#endif // NTP_MANAGER_H