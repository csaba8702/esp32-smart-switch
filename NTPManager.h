#ifndef NTP_MANAGER_H
#define NTP_MANAGER_H

#include <Arduino.h>
#include <time.h>
#include "WifiManager.h"

#define NTP_SERVER_1        "pool.ntp.org"
#define NTP_SERVER_2        "time.google.com"
#define NTP_TIMEZONE        "CET-1CEST,M3.5.0,M10.5.0/3"
#define NTP_RESYNC_INTERVAL 86400000UL  // 24 óra ms-ban

class NTPManager {
private:
    WifiManager& wifiManager;
    bool synced = false;
    unsigned long lastSyncTime = 0;  // millis() amikor utoljára szinkronizált

    bool checkSync() {
        time_t now = time(nullptr);
        return now > 1000000000UL;
    }

    bool doSync() {
        if (!wifiManager.isWifiConnected()) {
            Serial.println("[NTP] WiFi nincs – szinkron kihagyva.");
            return false;
        }
        configTzTime(NTP_TIMEZONE, NTP_SERVER_1, NTP_SERVER_2);
        unsigned long start = millis();
        while (!checkSync() && millis() - start < 10000) {
            delay(200);
        }
        if (checkSync()) {
            synced = true;
            lastSyncTime = millis();
            Serial.printf("[NTP] Szinkronizálva: %s\n", getDisplayString().c_str());
            return true;
        }
        Serial.println("[NTP] Szinkron sikertelen.");
        return false;
    }

public:
    NTPManager(WifiManager& wm) : wifiManager(wm) {}

    void begin() {
        Serial.println("[NTP] Inicializálás...");
        doSync();
    }

    void handle() {
        // 24 óránként újraszinkronizál, de csak ha van WiFi
        if (millis() - lastSyncTime >= NTP_RESYNC_INTERVAL) {
            Serial.println("[NTP] 24 órás újraszinkron...");
            doSync();
        }

        // Ha még nincs szinkronizálva (pl. boot-kori WiFi hiba), 30 mp-enként próbál
        if (!synced && wifiManager.isWifiConnected()) {
            static unsigned long lastRetry = 0;
            if (millis() - lastRetry >= 30000) {
                lastRetry = millis();
                Serial.println("[NTP] Újrapróbálkozás...");
                doSync();
            }
        }
    }

    bool isSynced() const { return synced; }

    struct tm getTime() {
        struct tm ti;
        time_t now = time(nullptr);
        localtime_r(&now, &ti);
        return ti;
    }

    time_t getTimestamp() { return time(nullptr); }

    // ISO dátum: "2026-05-17" (naptárhoz)
    String getDateISO() {
        if (!synced) return "----.--.--";
        struct tm ti = getTime();
        char buf[12];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
            ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday);
        return String(buf);
    }

    // Idő: "23:14:52"
    String getTimeStr() {
        if (!synced) return "--:--:--";
        struct tm ti = getTime();
        char buf[10];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
            ti.tm_hour, ti.tm_min, ti.tm_sec);
        return String(buf);
    }

    // ISO datetime: "2026-05-17T23:14:52" (JS Date() kompatibilis, naptárhoz)
    String getDateTimeISO() {
        if (!synced) return "";
        struct tm ti = getTime();
        char buf[22];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d",
            ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
            ti.tm_hour, ti.tm_min, ti.tm_sec);
        return String(buf);
    }

    // Megjelenítési string: "2026.05.17. 23:14:52"
    String getDisplayString() {
        if (!synced) return "Szinkronizálás...";
        struct tm ti = getTime();
        char buf[24];
        snprintf(buf, sizeof(buf), "%04d.%02d.%02d. %02d:%02d:%02d",
            ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
            ti.tm_hour, ti.tm_min, ti.tm_sec);
        return String(buf);
    }

    // Naptárhoz
    String getDayOfWeek() {
        if (!synced) return "";
        struct tm ti = getTime();
        const char* days[] = {"Vasárnap","Hétfő","Kedd","Szerda","Csütörtök","Péntek","Szombat"};
        return String(days[ti.tm_wday]);
    }

    String getMonthName() {
        if (!synced) return "";
        struct tm ti = getTime();
        const char* months[] = {
            "Január","Február","Március","Április","Május","Június",
            "Július","Augusztus","Szeptember","Október","November","December"
        };
        return String(months[ti.tm_mon]);
    }

    int getYear()   { return synced ? getTime().tm_year + 1900 : 0; }
    int getMonth()  { return synced ? getTime().tm_mon + 1 : 0; }
    int getDay()    { return synced ? getTime().tm_mday : 0; }
    int getHour()   { return synced ? getTime().tm_hour : 0; }
    int getMinute() { return synced ? getTime().tm_min : 0; }
    int getSecond() { return synced ? getTime().tm_sec : 0; }
    int getWeekday(){ return synced ? getTime().tm_wday : -1; }
};

#endif // NTP_MANAGER_H
