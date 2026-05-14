// WifiManager.h
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <functional>

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

class WifiManager {
private:
    WifiConfig config;
    bool isConnected = false;
    bool shouldRun   = true;
    bool debugMode   = true;

    TaskHandle_t reconnectTaskHandle = nullptr;
    static const uint16_t RECONNECT_INTERVAL = 5000;
    static const uint8_t  MAX_RETRIES        = 10;

    std::function<void()>              onConnectedCallback    = nullptr;
    std::function<void()>              onDisconnectedCallback = nullptr;
    std::function<void(const char*)>   onErrorCallback        = nullptr;

    void debugPrint(const String& message) {
        if (debugMode) Serial.println("[WiFi] " + message);
    }

    static void reconnectTask(void* parameter) {
        WifiManager* wm = static_cast<WifiManager*>(parameter);
        while (wm->shouldRun) {
            if (!wm->isConnected && WiFi.status() != WL_CONNECTED) {
                wm->debugPrint("Kapcsolat elveszett. Újracsatlakozás...");
                wm->connect();
            }
            vTaskDelay(pdMS_TO_TICKS(RECONNECT_INTERVAL));
        }
        vTaskDelete(nullptr);
    }

    void startReconnectTask() {
        xTaskCreate(reconnectTask, "WifiReconnect", 4096, this, 1, &reconnectTaskHandle);
        debugPrint("Reconnect task elindítva");
    }

public:
    WifiManager(const WifiConfig& wifiConfig) : config(wifiConfig) {}

    ~WifiManager() { close(); }

    void setDebugMode(bool enable) { debugMode = enable; }

    bool connect() {
        uint8_t attempts = 0;
        debugPrint("Csatlakozás: " + String(config.getSSID()));
        WiFi.mode(WIFI_STA);
        WiFi.begin(config.getSSID(), config.getPassword());

        while (WiFi.status() != WL_CONNECTED && attempts < MAX_RETRIES) {
            delay(1000);
            debugPrint("Próba " + String(attempts + 1) + "/" + String(MAX_RETRIES));
            attempts++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            isConnected = true;
            debugPrint("Csatlakozva! IP: " + WiFi.localIP().toString());
            if (onConnectedCallback) onConnectedCallback();
            return true;
        } else {
            isConnected = false;
            debugPrint("Sikertelen csatlakozás!");
            if (onErrorCallback) onErrorCallback("Sikertelen WiFi csatlakozás");
            return false;
        }
    }

    void begin() {
        debugPrint("WiFi manager inicializálása...");
        connect();
        startReconnectTask();
    }

    void close() {
        shouldRun = false;
        if (reconnectTaskHandle != nullptr) {
            vTaskDelete(reconnectTaskHandle);
            reconnectTaskHandle = nullptr;
        }
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        isConnected = false;
        if (onDisconnectedCallback) onDisconnectedCallback();
    }

    bool isWifiConnected() const { return isConnected && (WiFi.status() == WL_CONNECTED); }
    String getLocalIP()    const { return WiFi.localIP().toString(); }
    int32_t getRSSI()      const { return WiFi.RSSI(); }

    void setOnConnected(std::function<void()> cb)            { onConnectedCallback = cb; }
    void setOnDisconnected(std::function<void()> cb)         { onDisconnectedCallback = cb; }
    void setOnError(std::function<void(const char*)> cb)     { onErrorCallback = cb; }
};

#endif // WIFI_MANAGER_H
