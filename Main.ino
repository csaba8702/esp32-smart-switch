#include "WifiManager.h"
#include "WebManager.h"
#include "WebSocketManager.h"
#include "DeviceTypes.h"
#include "DeviceManager.h"
#include "SensorManager.h"
#include "EepromManager.h"

// --- WiFi beállítások ---
static WifiConfig wifiConfig("ARRIS-6D0C", "mQ7kNcL3hQcf");

// --- Modulok ---
static EepromManager    eepromManager;
static WifiManager      wifiManager(wifiConfig);
static DeviceManager    deviceManager;
static WebManager       webManager(wifiManager);
static WebSocketManager webSocketManager(deviceManager, wifiManager);
static SensorManager    sensorManager;

void setup() {
    Serial.begin(115200);
    eepromManager.begin();
    //eepromManager.clear();        // ← ide
    //eepromManager.saveWebPassword("admin");  // ← és ide
    deviceManager.setEeprom(eepromManager);
    deviceManager.begin();
    webManager.setEeprom(eepromManager);   // Auth bekötés
    wifiManager.begin();
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
