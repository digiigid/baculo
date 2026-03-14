#ifndef PCF8575_MANAGER_H
#define PCF8575_MANAGER_H

#include <Wire.h>

enum PinModePCF { PCF_INPUT, PCF_OUTPUT };

class PCF8575Manager {
private:
    uint8_t  _address;
    uint16_t _inputMask = 0xFFFF;         // 1 = Entrada, 0 = Salida
    uint16_t _lastReadData = 0xFFFF;      // Datos leídos del chip
    uint16_t _outputRegister = 0xFFFF;    // Estado deseado de las salidas
    bool     _connected = false;

public:
    PCF8575Manager(uint8_t address = 0x20) : _address(address) {}

    /**
     * @param inputMask Máscara de 16 bits: 1 para Entradas (Pull-up), 0 para Salidas.
     */
    bool begin(uint16_t inputMask = 0xFFFF) {
        _inputMask = inputMask;
        _outputRegister = 0xFFFF; // Por defecto, todo en alto (Salidas OFF / Pull-ups ON)
        
        Wire.beginTransmission(_address);
        if (Wire.endTransmission() == 0) {
            _connected = true;
            return write(0xFFFF); // Inicialización física
        }
        _connected = false;
        return false;
    }

    // --- GESTIÓN DE PINES ---

    void pinMode(uint8_t pin, PinModePCF mode) {
        if (pin > 15) return;
        if (mode == PCF_INPUT) bitSet(_inputMask, pin);
        else bitClear(_inputMask, pin);
    }

    PinModePCF getPinMode(uint8_t pin) {
        return (bitRead(_inputMask, pin)) ? PCF_INPUT : PCF_OUTPUT;
    }

    // --- ESCRITURA (SALIDAS) ---

    void digitalWrite(uint8_t pin, bool state) {
        if (pin > 15 || getPinMode(pin) == PCF_INPUT) return;
        
        if (state) bitSet(_outputRegister, pin);
        else bitClear(_outputRegister, pin);
        
        // Aplicamos la máscara de entradas para no desactivar los pull-ups
        write(_outputRegister | _inputMask);
    }

    // --- LECTURA (ENTRADAS) ---

    bool digitalRead(uint8_t pin) {
        update(); // Refresca los datos del chip
        return bitRead(_lastReadData, pin);
    }

    // --- COMUNICACIÓN I2C ---

    bool update() {
        Wire.requestFrom(_address, (uint8_t)2);
        if (Wire.available() == 2) {
            uint8_t low = Wire.read();
            uint8_t high = Wire.read();
            _lastReadData = low | (high << 8);
            _connected = true;
            return true;
        }
        _connected = false;
        return false;
    }

    bool write(uint16_t data) {
        Wire.beginTransmission(_address);
        Wire.write((uint8_t)(data & 0xFF));
        Wire.write((uint8_t)((data >> 8) & 0xFF));
        return (Wire.endTransmission() == 0);
    }

    bool read_pin(uint8_t bit) {
        if (bit > 15) return false; 
        return (_lastReadData & (1UL << bit)) != 0;
    }

    void write_pin(uint8_t bit, bool value) {
        if (bit > 15) return;

        // Solo permitimos escribir si el pin está configurado como salida
        // Si es entrada, forzamos que el bit sea 1 (para mantener pull-up)
        if (getPinMode(bit) == PCF_INPUT) {
            bitSet(_outputRegister, bit);
        } else {
            if (value) bitSet(_outputRegister, bit);
            else bitClear(_outputRegister, bit);
        }

        // Enviamos el registro OR la máscara de entradas 
        // Esto garantiza que los pines de entrada siempre reciban un '1' (requisito del PCF)
        write(_outputRegister | _inputMask);
    }

    /**
     * Invierte el estado de un pin de salida (si está en 1 pasa a 0, y viceversa).
     * @param pin Número de pin (0 a 15)
     */
    void toggleWrite(uint8_t pin) {
        if (pin > 15 || getPinMode(pin) == PCF_INPUT) return;

        // Invertimos el bit usando el operador XOR (^)
        _outputRegister ^= (1 << pin);

        // Escribimos el nuevo estado asegurando que las entradas mantengan su pull-up
        write(_outputRegister | _inputMask);
    }

    // --- ESTADO ---
    
    uint16_t getRawInputs() { return _lastReadData; }
    uint16_t getRawOutputs() { return _outputRegister; }
    bool isConnected() { return _connected; }
};

#endif