// 1. MINDENEK ELŐTT AZ EEPROM ÉS ALAPTÍPUSOK
#include "EepromManager.h"
#include "DeviceTypes.h"

// 2. HÁLÓZAT ÉS IDŐ
#include "WifiManager.h"
#include "NTPManager.h"

// 3. HARDVER MENEDZSEREK
#include "DeviceManager.h"
#include "ScheduleManager.h"

// 4. AUTENTIKÁCIÓ
#include "AuthManager.h"

// 5. KAPCSOLATI RÉTEG ÉS WEBFELÜLET
#include "WebManager.h"
#include "WebSocketManager.h"
#include "SensorManager.h"

// 6. WATCHDOG
#include <esp_task_wdt.h>

// ----------------------------------------------------------------
// Watchdog timeout
// ----------------------------------------------------------------
static const uint32_t LOOP_WDT_TIMEOUT_S = 10;

// --- WiFi beállítások ---
static WifiConfig wifiConfig("ARRIS-6D0C", "mQ7kNcL3hQcf");

// --- Globális modul példányok ---
static EepromManager    eepromManager;
static AuthManager      authManager(eepromManager);
static WifiManager      wifiManager(wifiConfig);
static DeviceManager    deviceManager;
static NTPManager       ntpManager(wifiManager);
static WebManager       webManager(wifiManager);
static WebSocketManager webSocketManager(deviceManager, wifiManager);
static SensorManager    sensorManager;
static ScheduleManager  scheduleManager(&deviceManager, &eepromManager);

void setup() {
    Serial.begin(115200);

    // ---- Watchdog inicializálása ----
    esp_task_wdt_config_t wdtCfg = {
        .timeout_ms     = LOOP_WDT_TIMEOUT_S * 1000,
        .idle_core_mask = 0,
        .trigger_panic  = true
    };
    esp_task_wdt_reconfigure(&wdtCfg);
    esp_task_wdt_add(NULL);
    Serial.printf("[WDT] Loop task watchdog beallitva (%d mp)\n", LOOP_WDT_TIMEOUT_S);

    // ---- EEPROM ----
    eepromManager.begin();
    // eepromManager.clear(); // jelszó reset szükség esetén

    // ---- Auth: alapértelmezett jelszó beállítása első indításkor ----
    authManager.begin();

    // ---- Eszközök ----
    deviceManager.setEeprom(eepromManager);
    deviceManager.begin();

    // ---- Web és WebSocket ----
    webManager.setAuth(authManager);
    webSocketManager.setEeprom(eepromManager);
    webSocketManager.setScheduleManager(scheduleManager);

    // ---- Hálózat ----
    wifiManager.begin();
    ntpManager.begin();
    webSocketManager.setNTP(ntpManager);

    // ---- Szolgáltatások indítása ----
    webManager.begin();
    webSocketManager.begin();
    sensorManager.begin();
    scheduleManager.begin();

    Serial.println("[Main] Rendszer sikeresen elindult.");
}

void loop() {
    // ---- Watchdog feed ----
    esp_task_wdt_reset();

    // A WebServer és WebSocket handlerek egymás után többször is
    // meghívódnak, hogy a bejövő kérések ne torlódjanak fel.
    // Ez különösen fontos gyors böngésző újratöltéseknél.
    webManager.handle();
    webSocketManager.handle();
    webManager.handle();     // második kör: ha az első alatt érkezett új kérés
    webSocketManager.handle();

    sensorManager.handle();
    ntpManager.handle();

    // yield(): átadja a vezérlést az ESP32 belső TCP/IP stack taskjának
    // (lwIP), hogy feldolgozhassa a hálózati csomagokat.
    // Gyors újratöltéseknél ez megakadályozza a socket buffer telítődését.
    yield();

    // Időzítési szabályok ellenőrzése másodpercenként
    if (ntpManager.isSynced()) {
        static unsigned long lastScheduleCheck = 0;
        unsigned long now = millis();
        if (now - lastScheduleCheck >= 1000) {
            lastScheduleCheck = now;
            scheduleManager.checkSchedules((uint32_t)time(nullptr));
        }
    }
}
