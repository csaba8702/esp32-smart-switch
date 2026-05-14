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
    
    const char* getSSID() const { return ssid.c_str(); }
    const char* getPassword() const { return password.c_str(); }
};

class WifiManager {
private:
    WifiConfig config;
    bool isConnected = false;
    bool shouldRun = true;
    bool debugMode = true;  // Debug mód alapértelmezetten bekapcsolva
    TaskHandle_t reconnectTaskHandle = nullptr;
    static const uint16_t RECONNECT_INTERVAL = 5000;
    static const uint8_t MAX_RETRIES = 10;
    
    std::function<void()> onConnectedCallback = nullptr;
    std::function<void()> onDisconnectedCallback = nullptr;
    std::function<void(const char*)> onErrorCallback = nullptr;

    void debugPrint(const String& message) {
        if (debugMode) {
            Serial.println("[WiFi] " + message);
        }
    }

    static void reconnectTask(void* parameter) {
        WifiManager* wifiManager = static_cast<WifiManager*>(parameter);
        while (wifiManager->shouldRun) {
            if (!wifiManager->isConnected && WiFi.status() != WL_CONNECTED) {
                wifiManager->debugPrint("Connection lost. Attempting to reconnect...");
                wifiManager->connect();
            }
            vTaskDelay(pdMS_TO_TICKS(RECONNECT_INTERVAL));
        }
        vTaskDelete(nullptr);
    }

    void startReconnectTask() {
        xTaskCreate(
            reconnectTask,
            "WifiReconnect",
            4096,
            this,
            1,
            &reconnectTaskHandle
        );
        debugPrint("Reconnect task started");
    }

public:
    WifiManager(const WifiConfig& wifiConfig) : config(wifiConfig) {}

    ~WifiManager() {
        close();
    }

    void setDebugMode(bool enable) {
        debugMode = enable;
        debugPrint(enable ? "Debug mode enabled" : "Debug mode disabled");
    }

    bool connect() {
        uint8_t attempts = 0;
        
        debugPrint("Connecting to WiFi network: " + String(config.getSSID()));
        WiFi.mode(WIFI_STA);
        WiFi.begin(config.getSSID(), config.getPassword());

        while (WiFi.status() != WL_CONNECTED && attempts < MAX_RETRIES) {
            delay(1000);
            debugPrint("Connecting... Attempt " + String(attempts + 1) + "/" + String(MAX_RETRIES));
            attempts++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            isConnected = true;
            debugPrint("Connected successfully!");
            debugPrint("IP address: " + WiFi.localIP().toString());
            debugPrint("Signal strength (RSSI): " + String(WiFi.RSSI()) + " dBm");
            if (onConnectedCallback) {
                onConnectedCallback();
            }
            return true;
        } else {
            isConnected = false;
            debugPrint("Connection failed!");
            if (onErrorCallback) {
                onErrorCallback("Failed to connect to WiFi");
            }
            return false;
        }
    }

    void begin() {
        debugPrint("Initializing WiFi manager...");
        connect();
        startReconnectTask();
        WiFi.begin(config.getSSID(), config.getPassword());
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
        }
        Serial.print("WiFi connected! IP address: ");
        Serial.println(WiFi.localIP());
    }

    void close() {
        debugPrint("Closing WiFi connection...");
        shouldRun = false;
        if (reconnectTaskHandle != nullptr) {
            vTaskDelete(reconnectTaskHandle);
            reconnectTaskHandle = nullptr;
        }
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        isConnected = false;
        debugPrint("WiFi connection closed");
        if (onDisconnectedCallback) {
            onDisconnectedCallback();
        }
    }

    bool isWifiConnected() const {
        return isConnected && (WiFi.status() == WL_CONNECTED);
    }

    String getLocalIP() const {
        return WiFi.localIP().toString();
    }

    int32_t getRSSI() const {
        return WiFi.RSSI();
    }

    void setOnConnected(std::function<void()> callback) {
        onConnectedCallback = callback;
    }

    void setOnDisconnected(std::function<void()> callback) {
        onDisconnectedCallback = callback;
    }

    void setOnError(std::function<void(const char*)> callback) {
        onErrorCallback = callback;
    }

    void printStatus() {
        if (isWifiConnected()) {
            debugPrint("=== WiFi Status ===");
            debugPrint("Connected to: " + String(config.getSSID()));
            debugPrint("IP address: " + getLocalIP());
            debugPrint("Signal strength (RSSI): " + String(getRSSI()) + " dBm");
            debugPrint("=================");
        } else {
            debugPrint("WiFi is not connected");
        }
    }
};

#endif // WIFI_MANAGER_H