#ifndef SD_MANAGER_H
#define SD_MANAGER_H

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include "systemData.h"
#include "icons.h"

extern SystemData sys;

class SDManager {
private:
    bool _ready;
    SPIClass* _sdSPI;
    unsigned long _lastCheck = 0;
    const unsigned long _checkInterval = 5000; 

public:
    SDManager() : _ready(false), _sdSPI(NULL) {}

    /**
     * Inicializa la tarjeta SD usando el bus SPI secundario
     */
    void begin() {
        Serial.println("Iniciando tarjeta SD...");
        
        // Creamos una instancia de SPI dedicada para la SD
        // ESP32-S3 permite asignar SPI a casi cualquier pin
        _sdSPI = new SPIClass(FSPI); // FSPI es el bus SPI2/3 disponible
        _sdSPI->begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);

        if (!SD.begin(SD_CS, *_sdSPI)) {
            Serial.println("Error: No se pudo montar la tarjeta SD");
            _ready = false;
            return;
        }

        uint8_t cardType = SD.cardType();
        if (cardType == CARD_NONE) {
            Serial.println("Error: No hay tarjeta SD insertada");
            _ready = false;
            return;
        }

        Serial.println("Tarjeta SD lista.");
        _ready = true;
    }

    void update() {
        unsigned long now = millis();
        if (now - _lastCheck < _checkInterval) return;
        _lastCheck = now;

        // Verificamos si la tarjeta sigue ahí
        uint8_t cardType = SD.cardType();
        if (cardType == CARD_NONE) {
            if (_ready) {
                Serial.println(F("[SD] Tarjeta extraída"));
                _ready = false;
            }
            sys.sdState = SD_NONE;
            sys.sdPresent = false;
        } else {
            _ready = true;
            sys.sdPresent = true;
            // Si el estado estaba en error o ninguno, lo ponemos en OK
            if (sys.sdState != SD_WRITE) sys.sdState = SD_OK;
        }
    }

    /**
     * Añade una línea de texto a un archivo
     */
    void log(const char* path, const char* msg) {
        if (!_ready) return;
        return;
        File f = SD.open(path, FILE_APPEND);
        if (f) {
            f.println(msg);
            f.close();
        } else {
            Serial.printf("Error al abrir %s para escribir\n", path);
        }
    }

    bool isReady() { 
        return _ready; 
    }
};

// Instancia global para ser usada en todo el proyecto
extern SDManager sdManager;

#endif