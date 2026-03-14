#ifndef SHT45_MANAGER_H
#define SHT45_MANAGER_H

#include <Wire.h>
#include <Adafruit_SHT4x.h>
#include "icons.h"
#include "systemData.h"

extern SystemData sys;

// Usamos constantes para evitar "números mágicos"
#define SHT45_I2C_ADDR 0x44 
#define SHT_UPDATE_INTERVAL 2000 // No necesitamos leer cada ms

class SHT45_Manager {
private:
    Adafruit_SHT4x sht45;
    TwoWire* _wire;
    
    bool _isPresent = false;
    float _temperature = 0.0f;
    float _humidity = 0.0f;
    float _dewPoint = 0.0f;
    unsigned long _lastRead = 0;

    // Optimización matemática: fórmula aproximada de Magnus (muy precisa entre 0-50°C)
    // Evitamos instanciar variables pesadas en cada llamada
    float calculateDewPoint(float t, float h) {
        if (h <= 0) return 0;
        const float a = 17.27f;
        const float b = 237.7f;
        float alpha = ((a * t) / (b + t)) + log(h / 100.0f);
        return (b * alpha) / (a - alpha);
    }

    // Comprobación ultra rápida sin bloquear el bus
    bool compruebaSensor() {
        _wire->beginTransmission(SHT45_I2C_ADDR);
        return (_wire->endTransmission() == 0);
    }

public:
    SHT45_Manager() : _wire(&Wire) {}

    bool begin(TwoWire *theWire = &Wire) {
        _wire = theWire;
        Serial.println("[SHT] Inicializando SHT45...");
        
        // Timeout de seguridad para el ESP32-S3
        _wire->setTimeOut(50); 

        if (!sht45.begin(_wire)) {
            _isPresent = false;
            return false;
        }

        sht45.setPrecision(SHT4X_HIGH_PRECISION);
        sht45.setHeater(SHT4X_NO_HEATER);
        _isPresent = true;
        Serial.println("[SHT] Sensor detectado y configurado.");
        return true;
    }

    void update() {
        unsigned long now = millis();
        if (now - _lastRead < SHT_UPDATE_INTERVAL) return;
        _lastRead = now;

        // 1. Verificación de presencia antes de pedir datos
        if (!compruebaSensor()) {
            if (_isPresent) { // Si antes estaba y ahora no
                _isPresent = false;
                _temperature = NAN; // Usamos NAN en lugar de 0 para distinguir de 0 grados reales
                _humidity = NAN;
                Serial.println("[SHT] ERROR: Sensor desconectado.");
            }
            return; 
        }

        // 2. Intento de lectura segura
        sensors_event_t humidity, temp;
        if (sht45.getEvent(&humidity, &temp)) {
            _temperature = temp.temperature;
            _humidity = humidity.relative_humidity;
            _dewPoint = calculateDewPoint(_temperature, _humidity);
            
            if (!_isPresent) {
                Serial.println("[SHT] Sensor recuperado.");
                _isPresent = true;
            }
        } else {
            _isPresent = false;
        }
        
        // 3. Sincronización con la estructura global del sistema (sys)
        if (_isPresent) {
            sys.tempInt = _temperature;
            sys.humidityInt = _humidity;
            sys.dewPointInt = _dewPoint;
            // Asignación de iconos según rangos de temperatura
            if (sys.tempInt < 10.0f)      sys.temp1IconIdx = TempState::T_0;
            else if (sys.tempInt < 25.0f) sys.temp1IconIdx = TempState::T_25;
            else if (sys.tempInt < 50.0f) sys.temp1IconIdx = TempState::T_50;
            else if (sys.tempInt < 75.0f) sys.temp1IconIdx = TempState::T_75;
            else                          sys.temp1IconIdx = TempState::T_100;
        } else {
            // Estado de fallo: Limpiamos valores y ponemos icono vacío
            sys.tempInt = 0.0f;
            sys.humidityInt = 0.0f;
            sys.dewPointInt = 0.0f;
            sys.temp1IconIdx = TempState::T_0;
        }
    }

    // Getters con protección
    float getTemperature() { return _isPresent ? _temperature : 0.0f; }
    float getHumidity()    { return _isPresent ? _humidity : 0.0f; }
    float getDewPoint()    { return _isPresent ? _dewPoint : 0.0f; }
    bool isPresent()       { return _isPresent; }
};

#endif