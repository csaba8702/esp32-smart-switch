#include "WebManager.h"
#include "WebSocketManager.h"
#include "WifiManager.h"
#include "GateController.h"
#include "SensorManager.h"

const int LED_PIN = 2;
static WifiConfig wifiConfig("ARRIS-6D0C", "mQ7kNcL3hQcf");
static WifiManager wifiManager(wifiConfig);
static SensorManager sensorManager;
static GateController gateController(sensorManager);
static WebManager webManager(wifiManager);
static WebSocketManager webSocketManager(gateController, wifiManager);

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    
    wifiManager.begin();
    webManager.begin();
    webSocketManager.begin();
    sensorManager.begin();
}

void loop() {
    webManager.handle();
    webSocketManager.handle();
    gateController.handle();
    sensorManager.handle();
}