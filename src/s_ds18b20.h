#ifndef DS18B20_MANAGER_H
#define DS18B20_MANAGER_H

#include <OneWire.h>
#include <DallasTemperature.h>
#include "systemData.h"

extern SystemData sys;

class DS18B20Manager {
private:
    uint8_t _pin;
    OneWire _oneWire;
    DallasTemperature _sensors;
    float _lastTemp = -127.0;
    
    // Temporizador interno para no bloquear el loop
    uint32_t _lastMillis = 0;
    const uint32_t _interval = 2000; // Leer cada 2 segundos es suficiente para baterías

public:
    DS18B20Manager(uint8_t pin = 15) : _pin(pin), _oneWire(pin), _sensors(&_oneWire) {}

    bool begin() {
        Serial.println("[DS18B20] Inicializando DS18B20...");

        _sensors.begin();
        
        // Configuración de precisión y modo no bloqueante
        _sensors.setResolution(10);
        _sensors.setWaitForConversion(false);         
        delay(50); // Estabilización del bus        
        int count = _sensors.getDeviceCount();
        if (count > 0) {
            // Pedimos la primera conversión para que sys no empiece en -127
            _sensors.requestTemperatures();
            _lastMillis = millis();
            Serial.printf("[DS18B20] Sensor detectado. Dispositivos encontrados: %d", count);
            Serial.println();
            return true;
        }
        Serial.println("[DS18B20] No se encontró ningún sensor DS18B20.");
        return false;
    }

    /**
     * Función Update: Se encarga de la lectura asíncrona.
     * Debe llamarse en cada iteración del loop principal.
     */
    void update() {
        uint32_t currentMillis = millis();

        // 1. Comprobar si ha pasado el intervalo
        if (currentMillis - _lastMillis >= _interval) {
            
            // 2. Recoger el resultado de la petición anterior
            float tempC = _sensors.getTempCByIndex(0);

            // 3. Validar y actualizar sys.tempExt
            if (tempC > -55.0 && tempC < 125.0 && tempC != 85.0) {
                _lastTemp = tempC;
                sys.tempExt = tempC; // Actualización directa del sistema
            } else if (tempC == -127.0) {
                // Si se desconecta, marcamos el error en sys
                sys.tempExt = -127.0;
            }

            // 4. Lanzar nueva petición (asíncrona) para la siguiente vuelta
            _sensors.requestTemperatures();
            _lastMillis = currentMillis;
        }
    }

    float getLastTemp() { return _lastTemp; }
    
    bool isConnected() {
        return _sensors.getDeviceCount() > 0;
    }
};

#endif