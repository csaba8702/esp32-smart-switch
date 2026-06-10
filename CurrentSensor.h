#ifndef CURRENT_SENSOR_H
#define CURRENT_SENSOR_H

#include <Arduino.h>
#include "BaseSensor.h"

// ----------------------------------------------------------------
// CurrentSensor – Árammérő szenzor
//
// Jelenleg: mock mód – Serial monitoron keresztül adható be érték:
//   Parancs: "csX:Y.YY"
//   Ahol X = sensorId, Y.YY = áram ampere-ben
//   Pl: "cs1:2.50" → az 1-es szenzornak 2.50A értéket ad
//
// Később: ACS712 ADC olvasással helyettesíthető, csak a
//   readHardware() metódust kell megírni, minden más marad.
//
// Riasztás feltételek:
//   - CURRENT_LOSS: relé BE van kapcsolva, de áram < minExpected
//   - OVERCURRENT:  áram > setpoint + toleranceHigh
//   - UNDERCURRENT: áram < setpoint - toleranceLow (ha > 0)
// ----------------------------------------------------------------

class CurrentSensor : public BaseSensor {
private:
    // Konfiguráció
    uint8_t  _pin;             // ADC pin (mock módban nem használt)
    float    _setpoint;        // elvárt áramérték (A)
    float    _toleranceLow;    // mekkora hiány megengedett (pozitív szám, pl 0.5)
    float    _toleranceHigh;   // mekkora túllépés megengedett (pozitív szám, pl 1.0)
    float    _minExpected;     // ez alatt "megszakadt" riasztás (pl 0.1A)

    // Állapot
    float    _lastValue   = 0.0f;
    bool     _ready       = false;
    bool     _mockMode    = true;

    // Riasztás
    bool     _alertActive = false;
    char     _alertBuf[48] = "";

    // Külső relé állapot lekérdező – SensorManager állítja be
    // nullptr ha nincs bekötve
    bool* _pRelayState = nullptr;

    // ---- Mock: Serial parancs feldolgozás ----
    // A SensorManager hívja, nem ez az osztály olvassa a Serial-t
    // (hogy ne legyen verseny több példány között)
public:
    // Meghívja a SensorManager amikor "csX:Y.YY" parancsot kap
    void injectMockValue(float value) {
        if (_mockMode) {
            _lastValue = value;
            Serial.printf("[CurrentSensor %d] Mock ertek: %.2f A\n", sensorId, value);
        }
    }

private:
    float readHardware() {
        // TODO: ACS712 / INA219 ADC olvasás
        // float raw = analogRead(_pin);
        // return (raw - 2048) * (5.0f / 4096.0f) / 0.185f; // ACS712-05B
        return _lastValue; // mock: az utolsó beadott értéket adja vissza
    }

    void evaluateAlerts() {
        bool relayOn = (_pRelayState != nullptr) ? *_pRelayState : false;

        _alertActive = false;
        _alertBuf[0] = '\0';

        // 1. Megszakadás: relé BE van de nincs áram
        if (relayOn && _lastValue < _minExpected) {
            _alertActive = true;
            snprintf(_alertBuf, sizeof(_alertBuf),
                "Aramszakadas! Mert: %.2fA (min: %.2fA)", _lastValue, _minExpected);
            return;
        }

        // 2. Túláram: setpoint + toleranceHigh felett
        if (_setpoint > 0.0f && _lastValue > _setpoint + _toleranceHigh) {
            _alertActive = true;
            snprintf(_alertBuf, sizeof(_alertBuf),
                "Tularam! Mert: %.2fA (max: %.2fA)", _lastValue, _setpoint + _toleranceHigh);
            return;
        }

        // 3. Alulfogyasztás: relé BE, setpoint megvan, de keveset fogyaszt
        //    (pl. fűtőfólia részleges törés)
        if (relayOn && _setpoint > 0.0f && _toleranceLow > 0.0f
            && _lastValue < _setpoint - _toleranceLow
            && _lastValue >= _minExpected)
        {
            _alertActive = true;
            snprintf(_alertBuf, sizeof(_alertBuf),
                "Alacsony aramertek! Mert: %.2fA (min: %.2fA)",
                _lastValue, _setpoint - _toleranceLow);
        }
    }

public:
    // ----------------------------------------------------------------
    // Konstruktor
    //   pin:            ADC pin (mock módban nem számít, adj meg 0-t)
    //   setpoint:       normál működési áram (A), 0 = csak megszakadás figyelés
    //   toleranceLow:   mennyivel lehet kevesebb (A), pl 0.5
    //   toleranceHigh:  mennyivel lehet több (A), pl 1.0
    //   minExpected:    ez alatt "nincs áram" (A), pl 0.1
    // ----------------------------------------------------------------
    CurrentSensor(uint8_t pin,
                  float setpoint      = 0.0f,
                  float toleranceLow  = 0.5f,
                  float toleranceHigh = 1.0f,
                  float minExpected   = 0.1f)
        : _pin(pin),
          _setpoint(setpoint),
          _toleranceLow(toleranceLow),
          _toleranceHigh(toleranceHigh),
          _minExpected(minExpected)
    {}

    // ---- BaseSensor interface implementáció ----

    const char* getType()  override { return "CURRENT"; }
    const char* getUnit()  override { return "A"; }

    void begin() override {
        _ready = true;
        _mockMode = true; // TODO: false ha fizikai szenzor bekötve
        Serial.printf("[CurrentSensor %d] Inicializalva (relayUUID=%06X, pin=%d, mock=%s)\n",
            sensorId, relayUUID, _pin, _mockMode ? "igen" : "nem");
        Serial.printf("  Setpoint: %.2fA  TolLow: %.2fA  TolHigh: %.2fA  MinExp: %.2fA\n",
            _setpoint, _toleranceLow, _toleranceHigh, _minExpected);
        if (_mockMode) {
            Serial.printf("  Mock parancs: cs%d:2.50 (pl. 2.50 Ampert ad be)\n", sensorId);
        }
    }

    void handle() override {
        if (!_ready) return;
        if (!_mockMode) {
            _lastValue = readHardware();
        }
        evaluateAlerts();
    }

    float read()    override { return _lastValue; }
    bool  isReady() override { return _ready; }

    float getSetpoint()      override { return _setpoint; }
    float getToleranceLow()  override { return _toleranceLow; }
    float getToleranceHigh() override { return _toleranceHigh; }

    bool        isAlert()    override { return _alertActive; }
    const char* getAlertMsg()override { return _alertBuf; }

    // ---- Konfiguráció futás közben ----
    void setSetpoint(float v)      { _setpoint      = v; }
    void setToleranceLow(float v)  { _toleranceLow  = v; }
    void setToleranceHigh(float v) { _toleranceHigh = v; }
    void setMinExpected(float v)   { _minExpected    = v; }
    void setMockMode(bool v)       { _mockMode = v; }

    // Relé állapot pointer bekötése (SensorManager hívja)
    void linkRelayState(bool* pState) { _pRelayState = pState; }

    // Fizikai pin lekérdezése
    uint8_t getPin() const { return _pin; }
};

#endif // CURRENT_SENSOR_H
