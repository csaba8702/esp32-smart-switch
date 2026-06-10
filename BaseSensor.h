#ifndef BASE_SENSOR_H
#define BASE_SENSOR_H

#include <Arduino.h>

// ----------------------------------------------------------------
// BaseSensor – absztrakt interface minden szenzor osztályhoz
//
// Új szenzor hozzáadása:
//   1. Hozz létre egy új .h fájlt (pl. TempSensor.h)
//   2. Örökölj ebből: class TempSensor : public BaseSensor
//   3. Implementáld a pure virtual metódusokat
//   4. A SensorManager automatikusan kezeli
// ----------------------------------------------------------------

class BaseSensor {
public:
    // Metaadatok – SensorManager állítja be regisztrációkor
    uint8_t  sensorId   = 0;         // egyedi azonosító (1-based)
    uint32_t relayUUID  = 0;         // melyik reléhez tartozik (UUID, állandó)
    bool     active     = true;

    virtual ~BaseSensor() {}

    // ---- Kötelezően implementálandó ----
    virtual const char* getType()  = 0;  // pl. "CURRENT", "TEMP"
    virtual void        begin()    = 0;  // inicializálás
    virtual void        handle()   = 0;  // polling / frissítés
    virtual float       read()     = 0;  // aktuális mért érték
    virtual bool        isReady()  = 0;  // működik-e

    // ---- Opcionális – mock értékkel ha nincs értelme ----
    virtual float getSetpoint()     { return 0.0f; }  // célérték / referencia
    virtual float getToleranceLow() { return 0.0f; }  // alsó tűrés (negatív irány)
    virtual float getToleranceHigh(){ return 0.0f; }  // felső tűrés (pozitív irány)

    // ---- Riasztás logika – subclass dönti el ----
    virtual bool        isAlert()    { return false; }
    virtual const char* getAlertMsg(){ return ""; }

    // ---- Egységek megjelenítéshez ----
    virtual const char* getUnit()    { return ""; }   // pl. "A", "°C", "V"
};

#endif // BASE_SENSOR_H
