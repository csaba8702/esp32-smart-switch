// 1. MINDENEK ELŐTT AZ EEPROM ÉS ALAPTÍPUSOK
#include "EepromManager.h"
#include "DeviceTypes.h"

// 2. HÁLÓZAT ÉS IDŐ
#include "WifiManager.h"
#include "NTPManager.h"

// 3. HARDVER MENEDZSEREK (amik már függenek az EEPROM-tól)
#include "DeviceManager.h"
#include "ScheduleManager.h"

// 4. KNEKCIÓS RÉTEG ÉS WEBFELÜLET
#include "WebManager.h"
#include "WebSocketManager.h"
#include "SensorManager.h"

// --- WiFi beállítások ---
static WifiConfig wifiConfig("ARRIS-6D0C", "mQ7kNcL3hQcf");

// --- Globális Modul példányosítások ---
static EepromManager    eepromManager;
static WifiManager      wifiManager(wifiConfig);
static DeviceManager    deviceManager;
static NTPManager       ntpManager(wifiManager);
static WebManager       webManager(wifiManager);
static WebSocketManager webSocketManager(deviceManager, wifiManager);
static SensorManager    sensorManager;
static ScheduleManager  scheduleManager(&deviceManager, &eepromManager);

void setup() {
    Serial.begin(115200);
    
    // Először elindítjuk az EEPROM-ot
    eepromManager.begin();
    
    // Szükség esetén törölhető (egyszeri futtatás jelszó resetre):
    //eepromManager.clear(); 

    // Összekötjük a függőségeket
    deviceManager.setEeprom(eepromManager);
    eepromManager.saveWebPassword("admin");
    deviceManager.begin();
    
    webManager.setEeprom(eepromManager);
    webSocketManager.setEeprom(eepromManager);
    webSocketManager.setScheduleManager(scheduleManager);
    
    // Hálózati szolgáltatások indítása
    wifiManager.begin();
    ntpManager.begin();
    webSocketManager.setNTP(ntpManager);
    
    webManager.begin();
    webSocketManager.begin();
    sensorManager.begin();
    scheduleManager.begin();
    
    Serial.println("[Main] Rendszer sikeresen elindult.");
}

void loop() {
    webManager.handle();
    webSocketManager.handle();
    sensorManager.handle();
    ntpManager.handle(); // <--- Nagyon fontos, hogy az időszinkron éljen!
    
    // Időzítési szabályok futtatása 5 másodpercenként
    if (ntpManager.isSynced()) {
        static unsigned long lastScheduleCheck = 0;
        if (millis() - lastScheduleCheck >= 5000) {
            time_t now = time(nullptr);
            scheduleManager.checkSchedules((uint32_t)now);
            lastScheduleCheck = millis();
        }
    }
}