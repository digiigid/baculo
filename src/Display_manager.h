#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "icons.h"
#include "systemData.h"
#include "s_pcf8575.h"

extern Adafruit_SSD1306 display;
extern uint8_t SCREEN_ADDRESS;
extern PCF8575Manager pcf; 

// Inicializamos a nullptr para evitar punteros basura
IconManager* guiClock = nullptr;
IconManager* guiTemp1 = nullptr;
IconManager* guiTemp2 = nullptr;
IconManager* guiLux   = nullptr;
IconManager* guiPos   = nullptr;
IconManager* guiLan   = nullptr;
IconManager* guiSD    = nullptr;
IconManager* guiPcf   = nullptr;
IconManager* guiPower = nullptr;
IconManager* guiBat1  = nullptr;
IconManager* guiBat2  = nullptr;
IconManager* guiOut1  = nullptr;
IconManager* guiOut2  = nullptr;
IconManager* guiOut3  = nullptr;
IconManager* guiOut4  = nullptr;
IconManager* guiOut5  = nullptr;
IconManager* guiOut6  = nullptr;

IconManager* menuIcons[17];

/**
 * Dibuja un valor y su unidad alineados a la derecha.
 * La unidad siempre es tamaño 1 y desplaza al número hacia la izquierda.
 * @param x Inicio del bloque (0 o 64)
 * @param y Posición vertical (línea de base)
 * @param width Ancho del bloque (64)
 * @param size Tamaño del número (1 o 2)
 * @param value Valor numérico
 * @param unit Unidad ("V", "A", etc.)
 */
void drawRightAlignedValue(int16_t x, int16_t y, int16_t width, uint8_t size, float value, const char* unit) {
    char valStr[10];
    int numCharWidth = (size == 1) ? 6 : 12; 
    int unitWidth = strlen(unit) * 6; // Cada carácter de la unidad (size 1) ocupa 6px
    
    // 1. Calcular cuántos caracteres numéricos caben como máximo
    // Restamos el ancho de la unidad y un pequeño margen de 2px
    int availableWidthForNum = width - unitWidth - 2;
    int maxNumChars = availableWidthForNum / numCharWidth;

    // 2. Formatear el número con o sin decimales según el espacio restante
    snprintf(valStr, sizeof(valStr), "%.1f", value);
    if ((int)strlen(valStr) > maxNumChars) {
        snprintf(valStr, sizeof(valStr), "%.0f", value);
    }

    // 3. Calcular el ancho total del conjunto (Número + Unidad)
    int16_t totalWidth = (strlen(valStr) * numCharWidth) + unitWidth;
    
    // El inicio X se calcula restando el ancho total del margen derecho del bloque
    int16_t startX = x + width - totalWidth;

    // 4. Dibujar el Número (Tamaño 'size')
    display.setTextSize(size);
    display.setCursor(startX, y);
    display.print(valStr);

    // 5. Dibujar la Unidad (Tamaño 1)
    // Alineación vertical: si el número es grande (2), bajamos la unidad para que asiente en la base
    int16_t unitY = (size == 2) ? y + 8 : y; 
    display.setTextSize(1);
    display.setCursor(startX + (strlen(valStr) * numCharWidth), unitY);
    display.print(unit);
}


void drawFooter() {
    display.setTextSize(1);
    display.setCursor(0, 56);
}

void drawPowerData(const char* label, PowerMetrics &m) {
    display.setTextSize(1);
    display.setCursor(0, 20); display.print(label);        
    display.setCursor(80, 20); display.print(m.fuse ? "Fuse OK" : "Fuse FAIL");
    display.setTextSize(2); // 16 bit de alto
    drawRightAlignedValue(0, 32, 64, 2, m.voltage, "V");
    drawRightAlignedValue(64, 32, 64, 2, m.current, "A");
    drawRightAlignedValue(0, 48, 64, 2, m.power, "W");    
    drawRightAlignedValue(64, 48, 64, 2, m.energyAh, "Ah");
}

void drawBatteryData(const char* label, BatteryMetrics &m) {
    display.setTextSize(1);
    display.setCursor(0, 20); display.print(label);        
    
    if (!m.in_use) {
        display.setTextSize(2); // 16 bit de alto
        display.setCursor(0, 32); display.print("No instalada");
        return;
    } else {
        display.setTextSize(2); // 16 bit de alto        
        drawRightAlignedValue(0, 
            32, 64, 2, m.voltage, "V");
        drawRightAlignedValue(64, 32, 64, 2, m.current, "A");
        drawRightAlignedValue(0, 48, 42, 1, m.power, "W");    
        drawRightAlignedValue(42, 48, 42, 1, m.energyAh, "Ah");
        drawRightAlignedValue(85, 48, 42, 1, m.soc, "%");
    }
}

void drawIOBit(int x, int y, int bitIdx, uint16_t data, bool isInput) {
    // 1. Número de canal (Muy pequeño, encima de la caja)
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(x + (bitIdx < 10 ? 3 : 1), y - 8); // Ajuste de centrado para 2 dígitos
    display.print(bitIdx);

    // 2. Determinar estado activo (Lógica inversa: 0 físico = Activo/Cerrado)
    bool activo = !bitRead(data, bitIdx); 

    // 3. Dibujar casilla con letra invertida si está activo
    if (activo) {
        display.fillRect(x, y, 11, 11, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK); 
    } else {
        display.drawRect(x, y, 11, 11, SSD1306_WHITE);
        display.setTextColor(SSD1306_WHITE);
    }

    display.setCursor(x + 3, y + 2);
    display.print(isInput ? "I" : "O");
    display.setTextColor(SSD1306_WHITE);
}

bool iconsInit() {   
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("[DISP] ERROR: No se pudo inicializar OLED.");
        return false;
    }
    display.clearDisplay();
    Serial.println(F("[DISP] Pantalla lista."));

    // Iinicializa los iconos en la barra superior (posición y tipo)
    guiClock = new IconManager(&display, IconManager::CLOCK, 0, 0, 0,"System");
    guiTemp1 = new IconManager(&display, IconManager::TEMPERATURE, 0, 0, 1,"Int.T.");
    guiTemp2 = new IconManager(&display, IconManager::TEMPERATURE, 0, 0, 2,"Ext.T.");
    guiLux   = new IconManager(&display, IconManager::LUX, 0, 0, 3,"Light");
    guiPos = new IconManager(&display, IconManager::ORIENTATION, 0, 0, 4,"Angle");
    guiLan = new IconManager(&display, IconManager::LAN, 0, 0, 5,"Lan");
    guiSD = new IconManager(&display, IconManager::SD_CARD, 0, 0, 6,"SD Crd");      
    guiPcf = new IconManager(&display, IconManager::LAN, 0, 0, 7,"I/O Md");
    guiPower = new IconManager(&display, IconManager::POWER, 0, 0, 8,"Total");
    guiBat1 = new IconManager(&display, IconManager::BATTERY, 0, 0, 9,"Main B");
    guiBat2 = new IconManager(&display, IconManager::BATTERY, 0, 0,10,"Sys B");
    guiOut1 = new IconManager(&display, IconManager::POWER, 0, 0,11,"Out 1");
    guiOut2 = new IconManager(&display, IconManager::POWER, 0, 0,12,"Out 2");
    guiOut3 = new IconManager(&display, IconManager::POWER, 0, 0,13,"Out 3");
    guiOut4 = new IconManager(&display, IconManager::POWER, 0, 0,14,"Out 4");
    guiOut5 = new IconManager(&display, IconManager::POWER, 0, 0,15,"Out 5");
    guiOut6 = new IconManager(&display, IconManager::POWER, 0, 0,16,"Out 6");

    menuIcons[0] = guiClock;
    menuIcons[1] = guiTemp1;
    menuIcons[2] = guiTemp2;
    menuIcons[3] = guiLux;
    menuIcons[4] = guiPos;
    menuIcons[5] = guiLan;
    menuIcons[6] = guiSD;
    menuIcons[7] = guiPcf;
    menuIcons[8] = guiPower;
    menuIcons[9 ] = guiBat1;
    menuIcons[10] = guiBat2;
    menuIcons[11] = guiOut1;
    menuIcons[12] = guiOut2;    
    menuIcons[13] = guiOut3;
    menuIcons[14] = guiOut4;
    menuIcons[15] = guiOut5;
    menuIcons[16] = guiOut6;    
    return true;
}

// Función solicitada: Indicar línea (0-7) y texto
void writeLine(uint8_t line, const char* text, bool clearFirst = false) {
    if (clearFirst) display.clearDisplay();
    else  display.fillRect(0, line * 8, 128, 8, SSD1306_BLACK);    
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, line * 8);
    display.print(text); // Usamos print en lugar de println para evitar saltos extra
    display.display();
}

void drawHeader() {
    display.setTextWrap(false);
    IconManager* activeIcon = nullptr;

    // 1. Buscamos automáticamente el icono que pertenece a la página actual
    for (int i = 0; i < sys.totalPages; i++) {
        if (menuIcons[i]->getIdMenu() == sys.currentPage-1) {
            activeIcon = menuIcons[i];
            break;
        }
    }

    if (activeIcon != nullptr) {
        // 2. Dibujar el icono activo en la posición del header (0,0)
        // Usamos el índice de icono global según la página
        int iconIdx = 0; 
        switch(sys.currentPage){
            case 0: iconIdx = sys.clockIconIdx; break;
            case 1: iconIdx = sys.temp1IconIdx; break;
            case 2: iconIdx = sys.temp2IconIdx; break;
            case 3: iconIdx = sys.posIconIdx; break;
            case 4: iconIdx = sys.lanIconIdx; break;
            case 5: iconIdx = sys.sdIconIdx; break;
            case 6: iconIdx = sys.powerIconIdx; break;
            case 7: iconIdx = sys.bat1IconIdx; break;
            case 8: iconIdx = sys.bat2IconIdx; break;
            case 9: iconIdx = sys.out1IconIdx; break;
            case 10: iconIdx = sys.out2IconIdx; break;
            case 11: iconIdx = sys.out3IconIdx; break;
            case 12: iconIdx = sys.out4IconIdx; break;
            case 13: iconIdx = sys.out5IconIdx; break;
            case 14: iconIdx = sys.out6IconIdx; break;
            default: iconIdx = 0; break;
        }               
        activeIcon->update(iconIdx, true);
        display.setTextSize(1);
        display.setCursor(20, 4);
        display.print(activeIcon->getTitle());
    }

    // 3. Indicador de página (Derecha del todo)
    // Formato: [8/9]    
    display.setTextSize(1);
    char pgStr[8];
    sprintf(pgStr, "%d/%d", sys.currentPage, sys.totalPages);
    int16_t pgWidth = strlen(pgStr) * 6;
    
    // Dibujamos un pequeño marco redondeado opcional para el "página/total"
    //    display.drawRoundRect(126 - pgWidth - 4, 2, pgWidth + 6, 11, 2, SSD1306_WHITE);
    display.setCursor(128 - pgWidth - 3, 4);
    display.print(pgStr);

    // 4. Línea divisoria inferior
    display.drawLine(0, 16, 128, 16, SSD1306_WHITE);
}

void updateDisplay() {
    display.clearDisplay();
    drawHeader();
   

    switch(sys.currentPage) {
        case 1:  
            display.setTextSize(1);
            display.setCursor(0, 20);
            display.print("Hora"); 
            display.setTextSize(2);
            display.fillRect(0, 32, 128, 16, SSD1306_BLACK);
            display.setCursor(0, 32); display.print(sys.timeStr);
            display.setCursor(0, 48); display.print(sys.dateStr);
            break;

        case 2:
            display.setTextSize(1);
            display.setCursor(0, 20);
            display.print("Clima interior"); 
            display.setCursor(0, 54);
            display.print("Rocio:"); 
            display.setTextSize(2);
            display.setCursor(0, 32); display.printf("%2.1fC  %0.f%%", sys.tempInt, sys.humidityInt);
            display.setCursor(64, 48); display.printf("%2.1fC", sys.dewPointInt);
            break;

        case 3: 
            display.setTextSize(1);
            display.setCursor(0, 20);
            display.print("Clima exterior"); 
            display.setCursor(0, 54);
            display.print("Rocio:"); 
            display.setTextSize(2);
            display.setCursor(0, 32); display.printf("%2.1fC  %0.f%%", sys.tempExt, sys.humidityExt);
            display.setCursor(64, 48); display.printf("%2.1fC", sys.dewPointExt);
            break;

        case 4: 
            display.setTextSize(1);
            display.setCursor(0, 20);
            display.print("LUXOMETRO"); 

            // Valor numérico
            display.setTextSize(2);
            display.setCursor(0, 32); 
            display.printf("%2.0f lx", sys.lux); // .0f es suficiente para Lux

            // Estado basado en rangos
            display.setTextSize(1);
            display.setCursor(0, 52);             
            if (sys.lux < 5.0f) display.print("NOCHE");
            else if (sys.lux < 400.0f) display.print("CREPUSCULO");
            else if (sys.lux < 2000.0f) display.print("NUBLADO.");
            else display.print("SOLEADO");
            break;
                            
        case 5: // Báculo y Viento
            display.setTextSize(1);
            display.setCursor(0, 20);
            display.print("Inclinacion"); 
            display.setTextSize(2);
            display.setCursor(0, 32); display.printf("%2.1f", sys.tiltAngle);            
            display.setTextSize(1);
            display.setCursor(0, 52); display.print(nivelesViento[sys.windLevel]);
            break;

        case 6: // LAN            
            display.setTextSize(1);
            display.setCursor(0, 20);
            display.print("Lan"); 
            
            // Verificamos si estamos conectados (asumiendo que 2 es el estado LAN_CONN o similar)
            if (sys.lanIconIdx == LanState::LAN_CONN || sys.lanIconIdx == LanState::LAN_DATA) { 
                display.setTextSize(1); // Tamaño pequeño para que quepa la info
                
                // Fila 1: IP
                display.setCursor(0, 32);
                display.print("IP:  "); 
                display.print(sys.ipAddress);
                
                // Fila 2: Gateway
                display.setCursor(0, 40);
                display.print("GW:  "); 
                display.print(sys.gateway);
                
                // Fila 3: DNS
                display.setCursor(0, 48);
                display.print("DNS: "); 
                display.print(sys.dns);

                // Fila 4: Máscara o Estado
                display.setCursor(0, 56);
                display.print("SUB: "); 
                display.print(sys.subnetMask);
                
            } else {
                // Estado sin conexión: Texto grande y centrado
                display.setTextSize(2);
                display.setCursor(20, 38);
                display.print("SIN RED");
            }
            break;

        case 7: // Almacenamiento
            display.setTextSize(1);
            display.setCursor(0, 20);
            display.print("ALMACENAMIENTO SD");      
            
            if (sys.sdPresent) {
                display.setTextSize(2);
                display.setCursor(0, 35);
                display.print("ESTADO: OK");
                
                display.setTextSize(1);
                display.setCursor(0, 55);
                // Mostramos el estado del enum para saber si está escribiendo
                if (sys.sdState == SD_WRITE) display.print("> ESCRIBIENDO...");
                else display.print("> LISTA PARA LOG");
            } else {
                display.setTextSize(2);
                display.setCursor(0, 35);
                display.setTextColor(SSD1306_WHITE); // Podrías usar parpadeo si quisieras
                display.print("SIN DISCO");
                
                display.setTextSize(1);
                display.setCursor(0, 55);
                display.print("Inserte tarjeta TF");
            }
            break; 

        case 8: {  // DIAGNÓSTICO PCF8575
            display.setTextSize(1);
            display.setCursor(0, 20);
            display.print("E/S");
            
            // Obtenemos el estado bruto (raw) del expansor
            uint16_t inputs = pcf.getRawInputs();
            uint16_t outputs = pcf.getRawOutputs();
            int startX = 18;
            int spacing = 13;
            
            // Fila A: P00 a P07 (Todas entradas en tu diseño)
            display.setCursor(0, 28); display.print("A)");
            for (int i = 0; i < 8; i++) {
                bool isInput = (pcf.getPinMode(i) == PCF_INPUT);
                uint16_t currentStatus = isInput ? inputs : outputs;                
                drawIOBit(startX + (i * spacing), 28, i, currentStatus, isInput);
            }

            // Fila B: P08 a P15 (P08-09 I, P10-15 O)
            display.setCursor(0, 48); display.print("B)");
            for (int i = 0; i < 8; i++) {
                int idx = i + 8;
                bool isInput = (pcf.getPinMode(idx) == PCF_INPUT);
                uint16_t currentStatus = isInput ? inputs : outputs;
                drawIOBit(startX + (i * spacing), 48, idx, currentStatus, isInput);
            }
            break;        
        }

        // Presenta las potencias y energías de las fuentes y baterías. De la 7 a la 10 son fuente, batería principal, batería sistema
        case 9:  drawPowerData("Power supply", sys.p_Sistema); break;
        case 10:  drawBatteryData("Main battery", sys.bateria[0]); break;
        case 11: drawBatteryData("System Battery", sys.bateria[1]); break;

        default: // Cargas (ahora de la 11 a la 18)
            if (sys.currentPage >= 12 && sys.currentPage <= 17) {
                char title[16];
                snprintf(title, sizeof(title), "Load %d", sys.currentPage - 11);
                drawPowerData(title, sys.p_circuitos[sys.currentPage - 11]);
            }
            break;
    }

    drawFooter();
    display.display();
}


#endif