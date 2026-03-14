#ifndef BH1750_MANAGER_H
#define BH1750_MANAGER_H

#include <Wire.h>
#include <hp_BH1750.h> // Librería recomendada por su manejo de auto-rango
#include "systemData.h"

extern SystemData sys;

class BH1750_Manager {
private:
    hp_BH1750 _luxSensor;
    bool _ready = false;
    unsigned long _lastRead = 0;
    const unsigned long _updateInterval = 1000; // Leer cada segundo es suficiente
    const uint8_t _addr = 0x23; // Dirección por defecto (ADDR a GND)

    bool compruebaSensor() {
        Wire.beginTransmission(_addr);
        return (Wire.endTransmission() == 0);
    }

public:
    BH1750_Manager() {}

    bool begin() {
        Serial.println(F("[BH1750] Inicializando..."));
        
        if (!compruebaSensor()) {
            _ready = false;
            return false;
        }

        // Inicializa con calidad alta y auto-rango
        if (!_luxSensor.begin(_addr)) {
            _ready = false;
            return false;
        }

        _ready = true;
        Serial.println(F("[BH1750] Sensor OK."));
        return true;
    }

    void update() {
        unsigned long now = millis();
        if (now - _lastRead < _updateInterval) return;
        _lastRead = now;

        if (!_ready) {
            // Intento de reconexión cada 5 segundos si falló
            if (now % 5000 < 100) { 
                begin();
            }
            sys.sensorLux = 0;
            return;
        }

        // Lectura del sensor
        if (compruebaSensor()) {
            _luxSensor.start(); // Inicia medición
            float lux = _luxSensor.getLux();
            
            // Volcado a la estructura global
            sys.lux = lux;
            sys.sensorLux = 1;
            _ready = true;

            // Lógica de iconos o estados basada en luz (opcional)
            // sys.isNight = (lux < 10.0f); 
        } else {
            _ready = false;
            sys.sensorLux = 0;
            sys.lux = 0.0f;
            Serial.println(F("[BH1750] Sensor perdido."));
        }
    }

    float getLux() { return sys.lux; }
    bool isPresent() { return _ready; }
};

#endif