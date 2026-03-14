#ifndef INCLINE_MANAGER_H
#define INCLINE_MANAGER_H

#include <BMI160Gen.h>
#include <Arduino.h>
#include <Wire.h>
#include "icons.h"
#include "systemData.h"

extern SystemData sys;

class InclineManager 
{
private:
    bool _ready = false; 
    float _vibration = 0.0f;
    float _peakAccel = 0.0f;
    float _currentAngle = 0.0f;
    unsigned long _lastUpdate = 0;
    unsigned long _lastAttempt = 0;
    bool _firstInitDone = false;

    const uint8_t _addr = 0x69;

    // 🔍 Verifica si el sensor responde en el bus I2C
    bool compruebaSensor()
    {
        Wire.beginTransmission(_addr);
        return (Wire.endTransmission() == 0);
    }

public:
    InclineManager() : _ready(false) {}

    bool begin()
    {
        if (!compruebaSensor()) return false;

        // 🚀 Inicialización: 0 indica modo I2C, seguido de la dirección
        BMI160.begin(0, _addr); 
        
        // Configura el rango a 2g para máxima sensibilidad en inclinación
        BMI160.setAccelerometerRange(2); 
        
        _ready = true;
        if (!_firstInitDone) {
            Serial.println("[BMI] Sensor inicializado correctamente.");
            _firstInitDone = true;
        }
        return true;
    }

    bool update() {
        unsigned long now = millis();

        if (!_ready) {
            // Intentar reconectar cada 2 segundos si el sensor se perdió
            if (now - _lastAttempt > 2000) {
                _lastAttempt = now;
                if (compruebaSensor()) {
                    begin(); 
                }
            }
            return false;
        }

        // ⏱️ Limitar la frecuencia de muestreo a 50Hz (cada 20ms)
        if (now - _lastUpdate < 20) return false;
        _lastUpdate = now;

        // 📥 Lectura de datos crudos usando tipos de 16 bits firmados
        int ax_raw, ay_raw, az_raw, gx, gy, gz;
        BMI160.readMotionSensor(ax_raw, ay_raw, az_raw, gx, gy, gz);
        
        // 🛡️ Verificación de datos válidos
        if (ax_raw == 0 && ay_raw == 0 && az_raw == 0) {
            _ready = false;
            return false;
        }

        // 📐 Conversión a Gs (16384 LSB/g para el rango de 2g)
        float ax = ax_raw / 16384.0f;
        float ay = ay_raw / 16384.0f;
        float az = az_raw / 16384.0f;

        // Cálculo del ángulo de inclinación respecto a la vertical
        float targetAngle = atan2(sqrt(ax * ax + ay * ay), az) * 180.0f / M_PI;
        
        // 🌊 Filtro complementario simple para suavizar el movimiento
        _currentAngle = (_currentAngle * 0.5f) + (targetAngle * 0.5f);

        // 📈 Cálculo de vibración (desviación de la gravedad estática 1.0g)
        float mag = sqrt(ax * ax + ay * ay + az * az);
        float delta = abs(mag - 1.0f);
        _vibration = (_vibration * 0.92f) + (delta * 0.08f);

        // 💥 Detección de picos (impactos)
        bool impacto = (delta > 0.6f && delta > _peakAccel);
        if (delta > _peakAccel) _peakAccel = delta;
        else _peakAccel *= 0.96f; // Decaimiento lento del pico

        // Actualizar datos del sistema
        sys.sensorBMI = 1;
        sys.tiltAngle = _currentAngle;

        // Clasificación del nivel de "viento" basado en vibración 🌬️
        if (_vibration < 0.02f)      sys.windLevel = 0;
        else if (_vibration < 0.05f) sys.windLevel = 1;
        else if (_vibration < 0.10f) sys.windLevel = 2;
        else if (_vibration < 0.20f) sys.windLevel = 3;
        else                         sys.windLevel = 4;

        return impacto;
    }

    // Métodos de acceso
    float getInclineAngle() { return _ready ? _currentAngle : -99.0f; }
    float getVibrationValue() { return _vibration; }
    bool isPresent() { return _ready; }

    const char *getWindLevelName()
    {
        if (_vibration < 0.02f) return "Calma";
        if (_vibration < 0.05f) return "Brisa";
        if (_vibration < 0.12f) return "Moderado";
        return "¡Fuerte!";
    }
};

#endif