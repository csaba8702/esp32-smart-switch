// WifiManager.h
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <esp_task_wdt.h>

// ----------------------------------------------------------------
// AP mód beállítások – hotspot ha a MODE_PIN földre van húzva
// ----------------------------------------------------------------
#define WIFI_MODE_PIN       0       // GPIO 0 (BOOT gomb) – LOW = AP mód
#define AP_SSID             "SmartRelay"
#define AP_PASSWORD         "smartrelay123"  // min. 8 karakter
#define AP_IP               192, 168, 4, 1
#define AP_GATEWAY          192, 168, 4, 1
#define AP_SUBNET           255, 255, 255, 0

class WifiConfig {
private:
    String ssid;
    String password;
public:
    WifiConfig(const char* wifi_ssid, const char* wifi_password)
        : ssid(wifi_ssid), password(wifi_password) {}
    const char* getSSID()     const { return ssid.c_str(); }
    const char* getPassword() const { return password.c_str(); }
};

enum class WifiMode {
    STA,  // Router csatlakozás
    AP    // Saját hotspot
};

class WifiManager {
private:
    WifiConfig   config;
    WifiMode     currentMode  = WifiMode::STA;
    bool         isConnected  = false;
    bool         shouldRun    = true;
    bool         debugMode    = true;
    char         localIPBuf[16] = "0.0.0.0";

    TaskHandle_t reconnectTaskHandle = nullptr;
    static const uint16_t RECONNECT_INTERVAL = 5000;
    static const uint8_t  MAX_RETRIES        = 10;

    void debugPrint(const char* message) const {
        if (debugMode) Serial.printf("[WiFi] %s\n", message);
    }

    // ---- AP mód indítása ----
    bool startAP() {
        debugPrint("AP mod inditasa...");
        WiFi.mode(WIFI_AP);

        IPAddress ip(AP_IP);
        IPAddress gateway(AP_GATEWAY);
        IPAddress subnet(AP_SUBNET);
        WiFi.softAPConfig(ip, gateway, subnet);

        if (!WiFi.softAP(AP_SSID, AP_PASSWORD)) {
            debugPrint("AP inditas sikertelen!");
            return false;
        }

        isConnected = true;
        WiFi.softAPIP().toString().toCharArray(localIPBuf, sizeof(localIPBuf));
        Serial.printf("[WiFi] AP mod aktiv. SSID: %s | IP: %s\n", AP_SSID, localIPBuf);
        return true;
    }

    // ---- STA mód csatlakozás ----
    // Nem blokkoló: 1 mp-es lépésekben ellenőriz, közben WDT reset
    bool connectSTA() {
        Serial.printf("[WiFi] Csatlakozas: %s\n", config.getSSID());
        // Előző kapcsolat tisztítása – memória szivárgás elkerülése
        WiFi.disconnect(false);
        vTaskDelay(pdMS_TO_TICKS(100));
        WiFi.mode(WIFI_STA);
        WiFi.begin(config.getSSID(), config.getPassword());

        for (uint8_t attempts = 0; attempts < MAX_RETRIES; attempts++) {
            esp_task_wdt_reset();
            vTaskDelay(pdMS_TO_TICKS(1000));

            if (WiFi.status() == WL_CONNECTED) {
                isConnected = true;
                WiFi.localIP().toString().toCharArray(localIPBuf, sizeof(localIPBuf));
                Serial.printf("[WiFi] Csatlakozva! IP: %s\n", localIPBuf);
                return true;
            }
            Serial.printf("[WiFi] Proba %d/%d\n", attempts + 1, MAX_RETRIES);
        }

        isConnected = false;
        debugPrint("Sikertelen csatlakozas!");
        return false;
    }

    static void reconnectTask(void* parameter) {
        WifiManager* wm = static_cast<WifiManager*>(parameter);
        esp_task_wdt_add(NULL);

        uint8_t  failCount    = 0;               // egymás utáni sikertelen próbák
        uint32_t retryDelayMs = wm->RECONNECT_INTERVAL; // 5000ms alap
        static const uint32_t MAX_RETRY_DELAY_MS = 300000; // max 5 perc

        while (wm->shouldRun) {
            esp_task_wdt_reset();

            // AP módban nincs szükség újracsatlakozásra
            if (wm->currentMode == WifiMode::AP) {
                vTaskDelay(pdMS_TO_TICKS(wm->RECONNECT_INTERVAL));
                continue;
            }

            // STA módban: kapcsolat ellenőrzés
            if (WiFi.status() == WL_CONNECTED) {
                // Kapcsolat helyreállt
                if (!wm->isConnected) {
                    wm->isConnected = true;
                    failCount       = 0;
                    retryDelayMs    = wm->RECONNECT_INTERVAL;
                    Serial.println("[WiFi] Kapcsolat helyreallva.");
                }
                vTaskDelay(pdMS_TO_TICKS(wm->RECONNECT_INTERVAL));
                continue;
            }

            // Nincs kapcsolat – próbálkozás
            wm->isConnected = false;
            failCount++;
            Serial.printf("[WiFi] Kapcsolat hianya (proba #%d), ujracsatlakozas %d mp mulva...\n",
                          failCount, retryDelayMs / 1000);

            bool ok = wm->connectSTA();
            if (!ok) {
                // Exponenciális backoff: 5s -> 10s -> 20s -> ... -> max 5 perc
                retryDelayMs = min((uint32_t)(retryDelayMs * 2), MAX_RETRY_DELAY_MS);
            } else {
                failCount    = 0;
                retryDelayMs = wm->RECONNECT_INTERVAL;
            }

            // Várakozás – darabokban hogy a WDT ne timeout-oljon
            uint32_t waited = 0;
            while (waited < retryDelayMs && wm->shouldRun) {
                esp_task_wdt_reset();
                vTaskDelay(pdMS_TO_TICKS(1000));
                waited += 1000;
            }
        }

        esp_task_wdt_delete(NULL);
        vTaskDelete(nullptr);
    }

public:
    WifiManager(const WifiConfig& wifiConfig) : config(wifiConfig) {}
    ~WifiManager() { close(); }

    void setDebugMode(bool enable) { debugMode = enable; }
    WifiMode getMode() const { return currentMode; }

    void begin() {
        // MODE_PIN beolvasása – LOW = AP mód
        // pinMode(WIFI_MODE_PIN, INPUT_PULLUP);
        // bool apMode = (digitalRead(WIFI_MODE_PIN) == LOW);
        bool apMode = false; // Hardkódolt AP mód – digitalRead-del cseréld le ha kész az áramkör

        if (apMode) {
            currentMode = WifiMode::AP;
            Serial.println("[WiFi] MODE_PIN LOW – AP mod valasztva.");
            startAP();
        } else {
            currentMode = WifiMode::STA;
            Serial.println("[WiFi] MODE_PIN HIGH – STA mod valasztva.");
            connectSTA();
        }

        // Reconnect task csak STA módban releváns, de AP-ban is futtatjuk
        // hogy ha kihúzzák a MODE_PIN-t és újraindul, észlelje
        xTaskCreate(reconnectTask, "WifiReconnect", 4096, this, 1, &reconnectTaskHandle);
    }

    // Közvetlen csatlakozás – visszafelé kompatibilitáshoz
    bool connect() { return connectSTA(); }

    void close() {
        shouldRun = false;
        if (reconnectTaskHandle != nullptr) {
            vTaskDelete(reconnectTaskHandle);
            reconnectTaskHandle = nullptr;
        }
        if (currentMode == WifiMode::AP) {
            WiFi.softAPdisconnect(true);
        } else {
            WiFi.disconnect(true);
        }
        WiFi.mode(WIFI_OFF);
        isConnected = false;
    }

    bool isWifiConnected() const {
        if (currentMode == WifiMode::AP) return isConnected;
        // Dupla ellenőrzés: flag ÉS valódi WiFi státusz
        bool realStatus = (WiFi.status() == WL_CONNECTED);
        if (isConnected && !realStatus) {
            // Flag és valóság eltér – valószínűleg éppen szakadt meg
            const_cast<WifiManager*>(this)->isConnected = false;
        }
        return isConnected && realStatus;
    }
    const char* getLocalIP() const { return localIPBuf; }
    int32_t     getRSSI()    const {
        if (currentMode == WifiMode::AP) return 0;
        return WiFi.RSSI();
    }

    // AP módban az info sávban mutassuk hogy hotspot módban vagyunk
    const char* getModeString() const {
        return (currentMode == WifiMode::AP) ? "AP" : "STA";
    }
};

#endif // WIFI_MANAGER_H
