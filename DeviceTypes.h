#ifndef DEVICE_TYPES_H
#define DEVICE_TYPES_H

// Eszköz típusok
enum class DeviceType {
    GENERIC,
    WATER_PUMP,
    GATE,
    LIGHT,
    MOTOR
};

// Opcionális modul típusok (skeleton - később implementálható)
enum class ModuleType {
    NONE,
    CURRENT_SENSOR,   // pl. ACS712
    TEMP_SENSOR,      // pl. DS18B20
    FLOW_SENSOR
};

// Egy eszköz leírója
struct DeviceConfig {
    uint8_t id;
    const char* name;
    DeviceType type;
    uint8_t relayPin;
    bool relayActiveLOW;  // true = LOW = BE (legtöbb relé modul ilyen)
    ModuleType module;    // skeleton - jelenleg nem aktív
};

#endif // DEVICE_TYPES_H
