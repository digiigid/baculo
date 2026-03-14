#ifndef INA3221_MANAGER_H
#define INA3221_MANAGER_H

#include <Wire.h>
#include "systemData.h"

extern SystemData sys; 

// Registros básicos del INA3221
#define INA3221_REG_CONFIG       0x00
#define INA3221_REG_SHUNTVOLTAGE 0x01 // Registro base para Shunt (Canal 1-3)
#define INA3221_REG_BUSVOLTAGE   0x02 // Registro base para Bus (Canal 1-3)
#define INA3221_REG_ID           0xFF


// Clase para gestionar el INA3221
class INA3221Manager {
private:
    uint8_t  _address;
    bool     _connected = false;
    float    _shuntResistor[3]; // Ohms por canal
    float    _voltageDivider[3]; // Multiplicador (ej: 2.0 para 50%)

public:
    INA3221Manager(uint8_t address) : _address(address) {
        for(int i=0; i<3; i++) {
            _shuntResistor[i] = 0.1;  // Default 100mOhm
            _voltageDivider[i] = 1.0; // Default sin divisor
        }
    }

    bool begin() {
        Wire.beginTransmission(_address);
        if (Wire.endTransmission() == 0) {
            // Configuración básica: Promediado de 4 muestras, 1.1ms conversion
            // Escribimos al registro de configuración 0x00 el valor 0x7127
            writeRegister(INA3221_REG_CONFIG, 0x7127);
            _connected = true;
            return true;
        }
        _connected = false;
        return false;
    }

    // Configuración de hardware por canal (1-3)
    void setChannelConfig(uint8_t channel, float shunt, float divider = 1.0) {
        if (channel < 1 || channel > 3) return;
        _shuntResistor[channel - 1] = shunt;
        _voltageDivider[channel - 1] = divider;
    }

    // Lectura de Voltaje de BUS (V)
    float getVoltage(uint8_t channel) {
        if (!_connected || !updateConnection()) return 0.0f;
        
        int16_t raw = readRegister(INA3221_REG_BUSVOLTAGE + ((channel - 1) * 2));
        // El INA3221 entrega el Bus Voltage en pasos de 8mV
        return (float)raw * 0.001f * _voltageDivider[channel - 1];
    }

    // Lectura de Corriente (A)
    float getCurrent(uint8_t channel) {
        if (!_connected || !updateConnection()) return 0.0f;

        int16_t raw = readRegister(INA3221_REG_SHUNTVOLTAGE + ((channel - 1) * 2));
        // El voltaje de Shunt son pasos de 40uV (0.04mV)
        float shuntVoltage_V = (float)raw * 0.000040f;
        return shuntVoltage_V / _shuntResistor[channel - 1];
    }

    bool isConnected() { return _connected; }

private:
    bool updateConnection() {
        Wire.beginTransmission(_address);
        _connected = (Wire.endTransmission() == 0);
        return _connected;
    }

    void writeRegister(uint8_t reg, uint16_t value) {
        Wire.beginTransmission(_address);
        Wire.write(reg);
        Wire.write((value >> 8) & 0xFF);
        Wire.write(value & 0xFF);
        Wire.endTransmission();
    }

    int16_t readRegister(uint8_t reg) {
        Wire.beginTransmission(_address);
        Wire.write(reg);
        if (Wire.endTransmission() != 0) return 0;

        Wire.requestFrom(_address, (uint8_t)2);
        if (Wire.available() == 2) {
            uint16_t value = (Wire.read() << 8) | Wire.read();
            return (int16_t)value;
        }
        return 0;
    }
};


// Funciones para operaciones comunes con el INA3221
extern INA3221Manager inaBateria1;
extern INA3221Manager inaConsumoA;
extern INA3221Manager inaConsumoB;

// --- FUNCIONES DE CÁLCULO ---

static int calcularSOC(float v) {
    if (v >= 25.4) return 100;
    if (v <= 22.0) return 0;
    return (int)((v - 22.0) * (100.0 / (25.4 - 22.0)));
}

void actualizarMedidas() {
    static unsigned long _lastT = 0;
    unsigned long now = millis();
    if (_lastT == 0) { _lastT = now; return; }
    
    float dtH = (now - _lastT) / 3600000.0f;
    _lastT = now;

    // 1. INA 0x40 - BATERÍA MAESTRA
    if (inaBateria1.isConnected()) {
        sys.bateria[0].in_use = true;
        sys.bateria[0].current = inaBateria1.getCurrent(1);
        sys.bateria[0].voltage = inaBateria1.getVoltage(3);
        sys.bateria[0].voltageBattery1 = inaBateria1.getVoltage(2);
        sys.bateria[0].voltageBattery2 = sys.bateria[0].voltage - sys.bateria[0].voltageBattery1;
        sys.bateria[0].power = sys.bateria[0].voltage * sys.bateria[0].current;
        sys.bateria[0].energyAh += abs(sys.bateria[0].power) * dtH;
        sys.bateria[0].soc = calcularSOC(sys.bateria[0].voltage / 2.0f);
        
        sys.bateria[0].is_plugged = (sys.bateria[0].voltage > 25.5f);
        sys.bateria[0].is_charging = (sys.bateria[0].current > 0.05f);

        if (sys.bateria[0].is_charging) {
            // Si el cargador está activo (flujo de corriente positivo)
            sys.bat1IconIdx = BatState::BAT_CHG;
        } 
        else if (sys.bateria[0].is_plugged && sys.bateria[0].soc >= 99.0f) {
            // Si está enchufado pero ya no carga (batería llena)
            sys.bat1IconIdx = BatState::BAT_PLUG;
        }
        else {
            // Lógica por rangos de SOC (Estado de Carga)
            if (sys.bateria[0].soc < 15.0f)      sys.bat1IconIdx = BatState::BAT_CRITIC;
            else if (sys.bateria[0].soc < 35.0f) sys.bat1IconIdx = BatState::BAT_25;
            else if (sys.bateria[0].soc < 60.0f) sys.bat1IconIdx = BatState::BAT_50;
            else if (sys.bateria[0].soc < 85.0f) sys.bat1IconIdx = BatState::BAT_75;
            else                                sys.bat1IconIdx = BatState::BAT_100;
        }

    } else {
        sys.bateria[0].in_use = false;
        sys.bateria[0].is_charging = false;
        sys.bateria[0].is_plugged = false;
        sys.bat1IconIdx = BatState::BAT_ERR;

        sys.bateria[0].current = 0.0f;
        sys.bateria[0].voltage = 0.0f;
        sys.bateria[0].power = 0.0f;
    }

    // 2. INA 0x41 y 0x42 - CONSUMOS
    // Creamos un array de punteros para actualizar los outXIconIdx de forma indexada
    PwrState* outIcons[] = { &sys.out1IconIdx, &sys.out2IconIdx, &sys.out3IconIdx, &sys.out4IconIdx, &sys.out5IconIdx, &sys.out6IconIdx };

    for (int i = 0; i < 3; i++) {
        // --- Bloque A (Circuitos 0, 1, 2) ---
        if (inaConsumoA.isConnected()) {
            sys.p_circuitos[i].fuse = sys.fusible[i]; 
            sys.p_circuitos[i].voltage = inaConsumoA.getVoltage(i+1);
            sys.p_circuitos[i].current = inaConsumoA.getCurrent(i+1);   
            sys.p_circuitos[i].power = sys.p_circuitos[i].voltage * sys.p_circuitos[i].current;
            sys.p_circuitos[i].energyAh += (sys.p_circuitos[i].current * dtH);

            // Actualizar Icono basado en el % de carga respecto al fusible
            float loadA = (sys.p_circuitos[i].fuse > 0) ? (sys.p_circuitos[i].current / sys.p_circuitos[i].fuse) * 100.0f : 0;
            if (loadA < 20.0f)      *outIcons[i] = PwrState::PWR_20;
            else if (loadA < 45.0f) *outIcons[i] = PwrState::PWR_40;
            else if (loadA < 70.0f) *outIcons[i] = PwrState::PWR_60;
            else if (loadA < 90.0f) *outIcons[i] = PwrState::PWR_80;
            else                    *outIcons[i] = PwrState::PWR_100;
        }

        // --- Bloque B (Circuitos 3, 4, 5) ---
        int idxB = i + 3;
        if (inaConsumoB.isConnected()) {
            sys.p_circuitos[idxB].fuse = sys.fusible[idxB]; 
            sys.p_circuitos[idxB].voltage = inaConsumoB.getVoltage(i+1);
            sys.p_circuitos[idxB].current = inaConsumoB.getCurrent(i+1);
            sys.p_circuitos[idxB].power = sys.p_circuitos[idxB].voltage * sys.p_circuitos[idxB].current;
            sys.p_circuitos[idxB].energyAh += (sys.p_circuitos[idxB].current * dtH);

            // Actualizar Icono basado en el % de carga respecto al fusible
            float loadB = (sys.p_circuitos[idxB].fuse > 0) ? (sys.p_circuitos[idxB].current / sys.p_circuitos[idxB].fuse) * 100.0f : 0;
            if (loadB < 20.0f)      *outIcons[idxB] = PwrState::PWR_20;
            else if (loadB < 45.0f) *outIcons[idxB] = PwrState::PWR_40;
            else if (loadB < 70.0f) *outIcons[idxB] = PwrState::PWR_60;
            else if (loadB < 90.0f) *outIcons[idxB] = PwrState::PWR_80;
            else                    *outIcons[idxB] = PwrState::PWR_100;
        }
    }
}


#endif