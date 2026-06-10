// ================================================================
// Smart Relay System – Main.ino
// ================================================================

// 1. MINDENEK ELŐTT AZ EEPROM ÉS ALAPTÍPUSOK
#include "EepromManager.h"
#include "DeviceTypes.h"
#include <EEPROM.h>

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

// 6. SZENZOR RÉTEG
#include "BaseSensor.h"
#include "CurrentSensor.h"
// #include "TempSensor.h"   // Ide kerül majd
#include "SensorManager.h"

// 7. WATCHDOG
#include <esp_task_wdt.h>

// ----------------------------------------------------------------
// Watchdog timeout
// ----------------------------------------------------------------
static const uint32_t LOOP_WDT_TIMEOUT_S = 10;

// ----------------------------------------------------------------
// WiFi beállítások
// ----------------------------------------------------------------
static WifiConfig wifiConfig("ARRIS-6D0C", "mQ7kNcL3hQcf");

// ----------------------------------------------------------------
// Globális modul példányok
// ----------------------------------------------------------------
static EepromManager    eepromManager;
static AuthManager      authManager(eepromManager);
static WifiManager      wifiManager(wifiConfig);
static DeviceManager    deviceManager;
static NTPManager       ntpManager(wifiManager);
static WebManager       webManager(wifiManager);
static WebSocketManager webSocketManager(deviceManager, wifiManager);
static SensorManager    sensorManager;
static ScheduleManager  scheduleManager(&deviceManager, &eepromManager);

// ----------------------------------------------------------------
// Globális változók
// ----------------------------------------------------------------
uint32_t bootCount = 0;
#define EEPROM_BOOT_COUNT_ADDR 900 // EEPROM 0. címét használjuk 4 bájton

// ----------------------------------------------------------------
// setup()
// ----------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    EEPROM.begin(1024);

    // Boot counter logikája:
    EEPROM.get(EEPROM_BOOT_COUNT_ADDR, bootCount);
    
    // Ha "szemetet" olvasnánk (első indítás), reseteljük 0-ra
    if (bootCount > 1000000) bootCount = 0; 
    
    bootCount++;
    EEPROM.put(EEPROM_BOOT_COUNT_ADDR, bootCount);
    EEPROM.commit();

    Serial.print("[System] Boot count: ");
    Serial.println(bootCount);
    Serial.print("[System] Reset reason: ");
    Serial.println(esp_reset_reason());
    delay(200);
    Serial.println("\n[Main] Rendszer indul...");

    // ---- Watchdog ----
    esp_task_wdt_config_t wdtCfg = {
        .timeout_ms     = LOOP_WDT_TIMEOUT_S * 1000,
        .idle_core_mask = 0,
        .trigger_panic  = true
    };
    esp_task_wdt_reconfigure(&wdtCfg);
    esp_task_wdt_add(NULL);
    Serial.printf("[WDT] Watchdog beallitva (%d mp)\n", LOOP_WDT_TIMEOUT_S);
    delay(400);
    // ---- EEPROM ----
    eepromManager.begin();
    //eepromManager.clear(); // EEPROM reset szükség esetén (magic: 0xB0)

    // ---- Auth ----
    authManager.begin();

    // ---- Alapértelmezett relay konfiguráció (első indításkor) ----
    // Ha az első slot inaktív → nincs még konfiguráció → beírjuk a 4 alapértelmezett relét
    {
        bool active; uint8_t pin, devType, modType; bool activeLow; uint32_t uuid = 0;
        eepromManager.loadRelayConfig(0, active, pin, activeLow, devType, modType, uuid);
        if (!active) {
            Serial.println("[Main] Relay config ures – alapertelmezett 4 rele betoltese...");
            const uint8_t defaultPins[4] = {16, 17, 18, 19};
            for (uint8_t i = 0; i < 4; i++) {
                eepromManager.saveRelayConfig(
                    i,
                    true,
                    defaultPins[i],
                    true,                          // activeLOW
                    (uint8_t)DeviceType::GENERIC,
                    (uint8_t)ModuleType::NONE
                );
                String existingName = eepromManager.loadRelayName(i + 1);
                if (existingName.isEmpty()) {
                    char buf[32];
                    snprintf(buf, 32, "Rele %d", i + 1);
                    eepromManager.saveRelayName(i + 1, String(buf));
                }
            }
            Serial.println("[Main] 4 alapertelmezett rele elmentve.");
        }
    }

    // ---- DeviceManager ----
    deviceManager.setEeprom(eepromManager);
    deviceManager.begin();

    // ---- SensorManager – alert callback bekötése ----
    sensorManager.setOnAlert([](uint32_t relayUUID, uint8_t sensorId,
                                 const char* type, float value,
                                 const char* msg) {
        Serial.printf("[ALERT] relayUUID=%06X sensor=%d type=%s value=%.2f msg=%s\n",
                      relayUUID, sensorId, type, value, msg);
        // TODO: webSocketManager.sendAlert(relayUUID, sensorId, type, value, msg);
    });

    // DeviceManager bekötése SensorManager-be (UUID→ID fordításhoz)
    sensorManager.setDeviceManager(deviceManager);

    // ---- Szenzorok regisztrálása ----
    // Csak akkor adjuk hozzá ha a relé létezik.
    // Paraméterek: CurrentSensor(pin, setpoint, tolLow, tolHigh, minExpected)
    //
    // Jelenleg mock módban működnek – Serial paranccsal adható be érték:
    //   cs1:2.50  → 1-es szenzornak 2.50A
    //   list      → összes szenzor kilistázása
    //
    // Ha a relé létezik az EEPROM-ban, regisztráljuk a szenzort
    // (A pin itt nem számít mock módban, adj meg 0-t)
    //
    // Példa: relé UUID alapján regisztrálás
    // Az UUID lekérhető: deviceManager.getUUIDasUint(relayId)
    // if (deviceManager.getRelayCount() >= 1) {
    //     uint32_t uuid1 = deviceManager.getUUIDasUint(1);
    //     CurrentSensor* cs1 = new CurrentSensor(0, 2.5f, 0.5f, 1.0f, 0.1f);
    //     sensorManager.registerSensor(cs1, uuid1);
    // }
    //
    // Ezeket majd a webes felületről kell konfigurálni és EEPROM-ból betölteni.
    // Egyelőre kommentben maradnak.

    // ---- WebManager / WebSocketManager ----
    webManager.setAuth(authManager);
    webSocketManager.setEeprom(eepromManager);
    webSocketManager.setScheduleManager(scheduleManager);
    webSocketManager.setSensorManager(sensorManager);  // alert broadcast

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
    Serial.println("[Main] Serial parancsok: cs1:2.50 | list");
}

// ----------------------------------------------------------------
// loop()
// ----------------------------------------------------------------
void loop() {
    // ---- Watchdog feed ----
    esp_task_wdt_reset();

    // ---- Web kiszolgálás (kétszer hogy ne torlódjon) ----
    webManager.handle();
    webSocketManager.handle();
    webManager.handle();
    webSocketManager.handle();

    // ---- Szenzor polling ----
    sensorManager.handle();

    // ---- NTP ----
    ntpManager.handle();

    // ---- Hálózati stack ----
    yield();

    // ---- Időzítők ellenőrzése másodpercenként ----
    if (ntpManager.isSynced()) {
        static unsigned long lastScheduleCheck = 0;
        unsigned long now = millis();
        if (now - lastScheduleCheck >= 1000) {
            lastScheduleCheck = now;
            scheduleManager.checkSchedules((uint32_t)time(nullptr));
        }
    }
}
