#ifndef SYSTEM_DATA_H
#define SYSTEM_DATA_H

#include <Arduino.h>
#include "icons.h"

struct PowerMetrics {
    int i_fuse = 0;   // indica la entrada correspondiente al fusible en el modulo pcf8575
    bool fuse = false; // estado del fusible (true = OK, false = fundido)
    float voltage = 0.0f;
    float current = 0.0f;
    float power = 0.0f;
    float energyAh = 0.0f;
};

struct BatteryMetrics {
    bool in_use = false;
    bool is_charging = false;
    bool is_plugged = false;
    float voltageBattery1 = 0.0f; // Voltaje de la bateria 1 (12V)
    float voltageBattery2 = 0.0f; // Voltaje de la bateria 2 (12V)
    float voltage = 0.0f;         // Voltaje total de la batería (24V)
    float current = 0.0f;      // Corriente principal
    float power = 0.0f;        // Potencia total
    float energyAh = 0.0f;     // Energía acumulada en Wh
    int soc = 0;              // Estado de carga (%)
};


struct ButtonDevice {
    uint8_t IOpin;  // Mismo numero para entrada y salida en los pcf8574
    bool isPressed;
    bool lastPressed;
    bool lightOn;
};

struct SystemData {
    // 1. Reloj
    uint8_t clockState = 0; // 0: Sin pila, 1: Hora OK, 2: Sync Cloud
    char timeStr[9] = "00:00:00";
    char dateStr[11] = "01/01/2026";

    // 2 & 3. Temperaturas
    float tempInt = -127.0f; 
    float tempExt = -127.0f; 
    float humidityInt = 0.0f;
    float humidityExt = 0.0f;
    float dewPointInt = 0.0f;
    float dewPointExt = 0.0f;

    // 4. Luminosidad
    float lux;            // El valor real medido (ej. 150.5 lx)
    uint8_t sensorLux;    // El estado del sensor (0: Error/Ausente, 1: OK)

    // 4. Báculo
    float tiltAngle = 0.0f;
    uint8_t sensorBMI = 0;   // 0: Error, 1: OK
    uint8_t windLevel = 0;   // 0: Calma, 1: Leve, 2: Mod, 3: Fuerte, 4: Tormenta

    // 6. LAN
    uint8_t lanState = 0;    // 0: No cable, 1: Cable No IP, 2: Internet OK
    String ipAddress = "0.0.0.0";
    String gateway = "0.0.0.0";
    String dns = "0.0.0.0";
    String subnetMask = "0.0.0.0";

    // 7. SD
    bool sdPresent = false;
    SdState sdState = SD_NONE; 

    // 8. Energía General
    PowerMetrics source;     // Fuente
    PowerMetrics mainBat;    // Batería Principal
    PowerMetrics sysBat;     // Batería Sistema
    PowerMetrics loads[8];   // Cargas 1 a 8

    // 9. Botones
    ButtonDevice buttonA;
    ButtonDevice buttonB;

    // 10. Estado de los Iconos, como enum de icons.h
    ClkState  clockIconIdx = CLK_SUMMER;
    TempState temp1IconIdx = T_25;
    TempState temp2IconIdx = T_25;
    LuxState  luxIconIdx = LUX_NIGHT;
    PosState  posIconIdx = POS_VERT;
    LanState  lanIconIdx = LAN_DISC;
    SdState   sdIconIdx = SD_NONE;
    LanState  pcfIconIdx = LAN_CONN;
    PwrState  powerIconIdx = PWR_20;
    BatState  bat1IconIdx = BAT_25;
    BatState  bat2IconIdx = BAT_25;
    PwrState  out1IconIdx = PWR_20;
    PwrState  out2IconIdx = PWR_20;
    PwrState  out3IconIdx = PWR_20;
    PwrState  out4IconIdx = PWR_20;
    PwrState  out5IconIdx = PWR_20;
    PwrState  out6IconIdx = PWR_20;

    // 11. Inicializa la pagina actual
    uint8_t currentPage = 1;
    const uint8_t totalPages = 15;

    // 12. Otras definiciones
    bool testLamp = false;

    bool fusible[6] = {true, true, true, true, true, true}; 
    bool lampState[4] = {false, false, false, false};
    bool ventState = false;

    // Batería y Sistema
    float vBatTotal;    // 24V (INA 0x40 - Ch3)
    float vBatMedia;    // 12V (INA 0x40 - Ch2)
    float iBat;         // Corriente principal (INA 0x40 - Ch1)
    float pBat;         // Potencia total (W)
    float eBat;         // Energía acumulada batería (Wh)
    int   soc;          // Estado de carga (%)

    // Canales Individuales (0x41 y 0x42)
    PowerMetrics p_circuitos[6]; // 6 canales de medición para las cargas, con su estado de fusible incluido
    PowerMetrics p_Sistema; // Calcula la potencia total extraida de la fuente  de alimentacion
    BatteryMetrics bateria[2]; // Datos específicos de la batería, incluyendo SOC y energía acumulada

};

// Instancia global
extern SystemData sys;

#endif // SYSTEM_DATA_H