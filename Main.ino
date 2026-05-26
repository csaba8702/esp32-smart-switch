// 1. MINDENEK ELŐTT AZ EEPROM ÉS ALAPTÍPUSOK
#include "EepromManager.h"
#include "DeviceTypes.h"

// 2. HÁLÓZAT ÉS IDŐ
#include "WifiManager.h"
#include "NTPManager.h"

// 3. HARDVER MENEDZSEREK (amik már függenek az EEPROM-tól)
#include "DeviceManager.h"
#include "ScheduleManager.h"

// 4. KAPCSOLATI RÉTEG ÉS WEBFELÜLET
#include "WebManager.h"
#include "WebSocketManager.h"
#include "SensorManager.h"

// 5. WATCHDOG
#include <esp_task_wdt.h>

// ----------------------------------------------------------------
// Watchdog timeout értékek
//   LOOP_WDT_TIMEOUT_S  – ha a loop() ennyi másodpercig nem feedeli
//                         a WDT-t, az ESP32 újraindul.
//                         A loop() handlerei (WebServer, WebSocket)
//                         normál esetben jóval 1 sec alatt lefutnak.
//
//   A WifiReconnect task saját timeoutját a WifiManager kezeli
//   (ld. WifiManager.h), itt csak a loop task-ot regisztráljuk.
// ----------------------------------------------------------------
static const uint32_t LOOP_WDT_TIMEOUT_S = 10;

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

    // ---- Watchdog inicializálása ----
    // Az esp_task_wdt_init az összes regisztrált taskot figyeli.
    // panic=true: timeout esetén crash dump + újraindulás (nem silent reset).
    esp_task_wdt_config_t wdtCfg = {
        .timeout_ms     = LOOP_WDT_TIMEOUT_S * 1000,
        .idle_core_mask = 0,    // idle taskokat ne figyelje
        .trigger_panic  = true  // timeout -> panic -> reboot
    };
    esp_task_wdt_reconfigure(&wdtCfg);

    // Loop task (az Arduino main task) regisztrálása a WDT-hez.
    // A WifiReconnect task saját magát regisztrálja induláskor (WifiManager).
    esp_task_wdt_add(NULL); // NULL = aktuális task (loop task)
    Serial.println("[WDT] Loop task watchdog beallitva (" 
                   + String(LOOP_WDT_TIMEOUT_S) + " mp)");

    // Először elindítjuk az EEPROM-ot
    eepromManager.begin();

    // Szükség esetén törölhető (egyszeri futtatás jelszó resetre):
    // eepromManager.clear();

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
