#include "WifiManager.h"
#include "WebManager.h"
#include "WebSocketManager.h"
#include "DeviceTypes.h"
#include "DeviceManager.h"
#include "SensorManager.h"
#include "EepromManager.h"
#include "NTPManager.h"

// --- WiFi beállítások ---
static WifiConfig wifiConfig("ARRIS-6D0C", "mQ7kNcL3hQcf");

// --- Modulok ---
static EepromManager    eepromManager;
static WifiManager      wifiManager(wifiConfig);
static DeviceManager    deviceManager;
static NTPManager       ntpManager(wifiManager);   // WiFi referencia átadva
static WebManager       webManager(wifiManager);
static WebSocketManager webSocketManager(deviceManager, wifiManager);
static SensorManager    sensorManager;

void setup() {
    Serial.begin(115200);
    eepromManager.begin();
    deviceManager.setEeprom(eepromManager);
    deviceManager.begin();
    webManager.setEeprom(eepromManager);
    wifiManager.begin();
    ntpManager.begin();                    // WiFi után!
    webSocketManager.setNTP(ntpManager);
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
    ntpManager.handle();
}
