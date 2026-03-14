#ifndef INCLINE_MANAGER_H
#define INCLINE_MANAGER_H

#include <DFRobot_BMI160.h>
#include "icons.h"
#include "systemData.h"

extern SystemData sys;

class InclineManager
{
private:
  DFRobot_BMI160 *_bmi;
  bool _ready = false;
  float _vibration = 0.0f;
  float _peakAccel = 0.0f;
  float _currentAngle = 0.0f;
  unsigned long _lastUpdate = 0;
  unsigned long _lastAttempt = 0;
  bool _firstInitDone = false;

  // Dirección I2C del BMI160 (0x69 o 0x68)
  const uint8_t _addr = 0x69;

  bool compruebaSensor()
  {
    Wire.beginTransmission(_addr);
    return (Wire.endTransmission() == 0);
  }

public:
  InclineManager() : _bmi(nullptr), _ready(false) {}

  bool begin()
  {
    if (_bmi == nullptr)
      _bmi = new DFRobot_BMI160();

    // Si no responde al ping I2C, ni lo intentamos
    if (!compruebaSensor())
      return false;

    if (_bmi->I2cInit(_addr) != 0)
    {
      _ready = false;
      return false;
    }

    _ready = true;
    if (_firstInitDone)
    {
      Serial.println("[BMI] Sensor recuperado.");
      // log_event(LOG_INFO, "[BMI] Sensor recuperado.");
    }
    _firstInitDone = true;
    return true;
  }

    // UNIFICADO: Una sola lectura I2C para todos los cálculos
  bool update() {
      unsigned long now = millis();
      bool _wasReady = _ready;

      // 1. RECONEXIÓN
      if (!_ready) {
          if (now - _lastAttempt > 2000) {
              _lastAttempt = now;
              if (compruebaSensor()) {
                  if (begin()) return false; 
              }
          }
          return false;
      }

      // 2. FRECUENCIA (50Hz)
      if (now - _lastUpdate < 20) return false;
      _lastUpdate = now;

      int16_t data[20] = {0};
      
      // 3. LECTURA Y DETECCIÓN AGRESIVA
      // Si la lectura falla o el sensor ya no responde al ping, cortamos YA.
      int status = _bmi->getAccelGyroData(data);
      bool busVivo = compruebaSensor();
      bool datosNulos = (data[3] == 0 && data[4] == 0 && data[5] == 0);

      if (status != 0 || !busVivo || datosNulos) {
          _ready = false;
          _lastAttempt = now;
          
          // Enviamos el log de inmediato 
          if (_wasReady) {
              if (!busVivo){
                Serial.println("[BMI] Sensor perdido");
                // log_event(LOG_ERROR, "[BMI] Sensor perdido");
              
              } 
              else if (datosNulos) {
                Serial.println("[BMI] Sensor perdido (Datos Nulos).");
                // log_event(LOG_ERROR, "[BMI] Sensor perdido (Datos Nulos).");
              }
          }
          return false;
      }

      // 4. PROCESAMIENTO (ax, ay, az)
      float ax = data[3] / 16384.0f;
      float ay = data[4] / 16384.0f;
      float az = data[5] / 16384.0f;

      // Ángulo instantáneo (Filtro 0.5f para que la mano y el OLED vayan a la par)
      float targetAngle = atan2(sqrt(ax * ax + ay * ay), az) * 180.0f / M_PI;
      _currentAngle = (_currentAngle * 0.5f) + (targetAngle * 0.5f);

      // Magnitud y Vibración
      float mag = sqrt(ax * ax + ay * ay + az * az);
      float delta = abs(mag - 1.0f);
      _vibration = (_vibration * 0.92f) + (delta * 0.08f);

      // Impacto
      bool impacto = (delta > 0.6f && delta > _peakAccel);
      if (delta > _peakAccel) _peakAccel = delta;
      else _peakAccel *= 0.96f;

      // 5. ACTUALIZACIÓN DE ESTRUCTURA GLOBAL SYS
      sys.sensorBMI = 1;         // Sensor OK
      sys.tiltAngle = _currentAngle;

      // Mapeo de vibración a windLevel (0 a 4)
      if (_vibration < 0.02f)      sys.windLevel = 0; // Calma
      else if (_vibration < 0.05f) sys.windLevel = 1; // Leve
      else if (_vibration < 0.10f) sys.windLevel = 2; // Mod
      else if (_vibration < 0.20f) sys.windLevel = 3; // Fuerte
      else                         sys.windLevel = 4; // Tormenta

      return impacto;
  }

  // --- Getters Optimizados (Ya no hacen cálculos, solo devuelven el estado) ---
  float getInclineAngle() { return _ready ? _currentAngle : -99.0f; }
  float getVibrationValue() { return _vibration; }
  float getPeakValue() { return _peakAccel; }
  bool isPresent() { return _ready; }
  bool detectImpact() { return _peakAccel > 0.6f; }

  const char *getWindLevel()
  {
    if (_vibration < 0.02f)
      return "Calma";
    if (_vibration < 0.05f)
      return "Brisa";
    if (_vibration < 0.12f)
      return "Moderado";
    return "¡VENTARRON!";
  }
};

#endif