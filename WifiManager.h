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
    bool connectSTA() {
        uint8_t attempts = 0;
        Serial.printf("[WiFi] Csatlakozas: %s\n", config.getSSID());
        WiFi.mode(WIFI_STA);
        WiFi.begin(config.getSSID(), config.getPassword());

        while (WiFi.status() != WL_CONNECTED && attempts < MAX_RETRIES) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_task_wdt_reset();
            Serial.printf("[WiFi] Proba %d/%d\n", attempts + 1, MAX_RETRIES);
            attempts++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            isConnected = true;
            WiFi.localIP().toString().toCharArray(localIPBuf, sizeof(localIPBuf));
            Serial.printf("[WiFi] Csatlakozva! IP: %s\n", localIPBuf);
            return true;
        }

        isConnected = false;
        debugPrint("Sikertelen csatlakozas!");
        return false;
    }

    static void reconnectTask(void* parameter) {
        WifiManager* wm = static_cast<WifiManager*>(parameter);
        esp_task_wdt_add(NULL);

        while (wm->shouldRun) {
            esp_task_wdt_reset();

            // AP módban nincs szükség újracsatlakozásra
            if (wm->currentMode == WifiMode::AP) {
                vTaskDelay(pdMS_TO_TICKS(wm->RECONNECT_INTERVAL));
                continue;
            }

            if (!wm->isConnected && WiFi.status() != WL_CONNECTED) {
                wm->debugPrint("Kapcsolat elveszett. Ujracsatlakozas...");
                wm->connectSTA();
            }
            vTaskDelay(pdMS_TO_TICKS(wm->RECONNECT_INTERVAL));
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

    bool        isWifiConnected() const {
        if (currentMode == WifiMode::AP) return isConnected;
        return isConnected && (WiFi.status() == WL_CONNECTED);
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
