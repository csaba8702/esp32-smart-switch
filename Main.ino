#include "WifiManager.h"
#include "WebManager.h"
#include "WebSocketManager.h"
#include "DeviceTypes.h"
#include "DeviceManager.h"
#include "SensorManager.h"

// --- WiFi beállítások ---
static WifiConfig wifiConfig("SSID", "JELSZO");

// --- Modulok ---
static WifiManager      wifiManager(wifiConfig);
static DeviceManager    deviceManager;
static WebManager       webManager(wifiManager);
static WebSocketManager webSocketManager(deviceManager, wifiManager);
static SensorManager    sensorManager;

void setup() {
    Serial.begin(115200);
    wifiManager.begin();
    deviceManager.begin();
    webManager.begin();
    webSocketManager.begin();
    sensorManager.begin();
    Serial.println("[Main] Rendszer elindult.");
    Serial.println("[Main] Webfelület: http://" + wifiManager.getLocalIP() + ":8080");
}

void loop() {
    webManager.handle();
    webSocketManager.handle();
    deviceManager.handle();
    sensorManager.handle();
}
