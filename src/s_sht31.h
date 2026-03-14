#ifndef SHT31_EXT_MANAGER_H
#define SHT31_EXT_MANAGER_H

#include <Wire.h>
#include <Adafruit_SHT31.h>
#include "icons.h"
#include "systemData.h"

extern SystemData sys;

// Dirección I2C típica del SHT31 es 0x44 (o 0x45 si el ADDR está a VCC)
#define SHT31_I2C_ADDR 0x45 
#define SHT_UPDATE_INTERVAL 2000 

class SHT31_Manager {
private:
    Adafruit_SHT31 sht31;
    TwoWire* _wire;
    
    bool _isPresent = false;
    float _temperature = 0.0f;
    float _humidity = 0.0f;
    float _dewPoint = 0.0f;
    unsigned long _lastRead = 0;

    float calculateDewPoint(float t, float h) {
        if (h <= 0) return 0;
        const float a = 17.27f;
        const float b = 237.7f;
        float alpha = ((a * t) / (b + t)) + log(h / 100.0f);
        return (b * alpha) / (a - alpha);
    }

    bool compruebaSensor() {
        _wire->beginTransmission(SHT31_I2C_ADDR);
        return (_wire->endTransmission() == 0);
    }

public:
    SHT31_Manager() : _wire(&Wire) {}

    bool begin(TwoWire *theWire = &Wire) {
        _wire = theWire; // Guardamos la referencia
        Serial.println("[SHT] Inicializando SHT31 Exterior...");
        
        _wire->setTimeOut(50); 

        // En esta librería, begin solo acepta la dirección. 
        // Si usas el Wire principal, se inicializa así:
        if (!sht31.begin(SHT31_I2C_ADDR)) { 
            _isPresent = false;
            return false;
        }

        sht31.heater(false);
        _isPresent = true;
        Serial.println("[SHT] SHT31 detectado y configurado.");
        return true;
    }

    void update() {
        unsigned long now = millis();
        if (now - _lastRead < SHT_UPDATE_INTERVAL) return;
        _lastRead = now;

        // 1. Verificación de presencia
        if (!compruebaSensor()) {
            if (_isPresent) {
                _isPresent = false;
                _temperature = NAN;
                _humidity = NAN;
                Serial.println("[SHT] ERROR: SHT31 Exterior desconectado.");
            }
            return; 
        }

        // 2. Lectura de datos (SHT31 usa métodos directos)
        float t = sht31.readTemperature();
        float h = sht31.readHumidity();

        if (!isnan(t) && !isnan(h)) {
            _temperature = t;
            _humidity = h;
            _dewPoint = calculateDewPoint(_temperature, _humidity);
            
            if (!_isPresent) {
                Serial.println("[SHT] SHT31 Exterior recuperado.");
                _isPresent = true;
            }
        } else {
            _isPresent = false;
        }
        
        // 3. Sincronización con estructura global EXTERIOR
        if (_isPresent) {
            sys.tempExt = _temperature;
            sys.humidityExt = _humidity; 
            sys.dewPointExt = _dewPoint;

            // Asignación de iconos para EXTERIOR (Icono 2)
            if (sys.tempExt < 10.0f)      sys.temp2IconIdx = TempState::T_0;
            else if (sys.tempExt < 25.0f) sys.temp2IconIdx = TempState::T_25;
            else if (sys.tempExt < 50.0f) sys.temp2IconIdx = TempState::T_50;
            else if (sys.tempExt < 75.0f) sys.temp2IconIdx = TempState::T_75;        
            else                          sys.temp2IconIdx = TempState::T_100;  
        } else {
            sys.tempExt = 0.0f;
            sys.temp2IconIdx = TempState::T_0;
        }
    }

    float getTemperature() { return _isPresent ? _temperature : 0.0f; }
    float getHumidity()    { return _isPresent ? _humidity : 0.0f; }
    float getDewPoint()    { return _isPresent ? _dewPoint : 0.0f; }
    bool isPresent()       { return _isPresent; }
};

#endif
