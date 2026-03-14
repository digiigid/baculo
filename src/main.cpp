#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <Wire.h>          
#include "com_eth.h"
#include "SDManager.h"

#ifndef Arduino_h
#define Arduino_h // Esto engaña a la librería BMI160 para que crea que ya incluyó arduino.h
#endif

/************************************************************************************
# REFERENCIA DE LA CONTROLADORA: https://www.waveshare.com/wiki/ESP32-S3-ETH
 


************************************************************************************/



// Pines para las conexiones
#define I2C_SDA 17
#define I2C_SCL 16
#define I2C_INT 18


#include "ds3231.h"
#include "systemData.h"
SystemData sys; // --- Estructura global para almacenar datos del sistema ---

SDManager sdManager; // Clase para manejo de SD, definida en src/SDManager.h

// --- Configura el display OLED ---
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Display_manager.h"
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
uint8_t SCREEN_ADDRESS = 0x3C;
Adafruit_SSD1306 display (SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
unsigned long lastDisplayUpdate = 0;
const unsigned long displayInterval = 500; // Refresco cada 0.5 segundos

// --- Configuración del botón BOOT ---
#define BOOT_BUTTON 0
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50; // milisegundos
bool lastButtonState = HIGH;
bool buttonState = HIGH;

// --- Configura el sensor de temperatura y humedad SHT45  INTERNO ---
#include "s_sht45.h"
SHT45_Manager shtManager;

#include "s_sht31.h"
SHT31_Manager sht31Manager;

// --- Configura el inclinómetro para medir la inclinación del báculo
#include "s_bmi160.h"
InclineManager incline;

// --- Configura el sensor de luz BH1750 para medir la luminosidad exterior
#include "s_bh1750.h"
BH1750_Manager luxManager;

#include "s_ds18b20.h"
DS18B20Manager tempExt(15); // Pin de datos para el DS18B20 exterior

#include "StatusLED.h"
StatusLED stLed(21);

#include "s_pcf8575.h"
PCF8575Manager pcf(0x20); 

#include "s_ina3221.h"
INA3221Manager inaBateria1(0x40); // Batería y voltajes
INA3221Manager inaConsumoA(0x41); // Circuitos 1-3
INA3221Manager inaConsumoB(0x42); // Circuitos 4-6

// Para descargar codigo directamente desde el repositorio de github
//--------------------------------------------------------------------------------------------
// --- CONFIGURACIÓN DE GITHUB ---
#include <HTTPClient.h> // Usaremos el wrapper de ESP32 que es más potente
#include <WiFiClientSecure.h> // Aunque sea cable, necesitamos la capa de seguridad para GitHub
#include <Update.h>
const char* URL_VERSION = "https://raw.githubusercontent.com/digiigid/baculo/main/version.txt";
const char* URL_BINARIO = "https://github.com/digiigid/baculo/releases/latest/download/firmware.bin";
#ifdef VERSION_AUTO
  const char* VERSION_ACTUAL = VERSION_AUTO;
#else
  const char* VERSION_ACTUAL = "0"; 
#endif
// Usamos NetworkClientSecure para que funcione sobre Ethernet
WiFiClientSecure cliente;

void ejecutarActualizacion() {
    // --- MENSAJE EN PANTALLA ---
    display.clearDisplay();
    display.setTextSize(2);          // Texto un poco más grande
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 10);
    display.println("ACTUALIZANDO");
    display.setTextSize(1);
    display.setCursor(0, 35);
    display.println("No apagues el equipo...");
    display.display();

    HTTPClient http;
    cliente.setInsecure(); // Saltamos la verificación de certificado para GitHub
    Serial.printf("[OTA] Iniciando descarga vía Cable (Versión %s)...\n", VERSION_ACTUAL);
    if (http.begin(cliente, URL_BINARIO)) {
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        int codigoHTTP = http.GET();

        if (codigoHTTP == HTTP_CODE_OK) {
            int tamanoArchivo = http.getSize();
            if (Update.begin(tamanoArchivo)) {
                Serial.printf("[OTA] Grabando %d bytes...\n", tamanoArchivo);
                size_t escrito = Update.writeStream(http.getStream());

                if (escrito == tamanoArchivo && Update.end()) {
                    Serial.println("[OTA] ¡Éxito! Reiniciando...");
                    delay(1000);
                    ESP.restart();
                }
            }
        }
        http.end();
    }
}
void buscarActualizacion() {
    HTTPClient http;
    cliente.setInsecure();
    
    Serial.println("[Info] Buscando actualización por cable...");
    if (http.begin(cliente, URL_VERSION)) {
        int codigoHTTP = http.GET();
        if (codigoHTTP == HTTP_CODE_OK) {
            String versionRemota = http.getString();
            versionRemota.trim();
            if (versionRemota != VERSION_ACTUAL) {
                ejecutarActualizacion();
            } else {
                Serial.println("[Info] Firmware al día.");
            }
        }
        http.end();
    }
}
//--------------------------------------------------------------------------------------------

// Funcion para controlar el botón y cambiar de página en el display
void checkButton() {
    bool reading = digitalRead(BOOT_BUTTON);
    if (reading != lastButtonState) {
        lastDebounceTime = millis();
    }
    if ((millis() - lastDebounceTime) > debounceDelay) {

        if (reading != buttonState) {
            buttonState = reading;
            if (buttonState == HIGH) {
                sys.currentPage++;
                if (sys.currentPage > 17) {
                    sys.currentPage = 1;
                }
                Serial.printf("[GUI] Cambio a página: %d", sys.currentPage);
                Serial.println();
                updateDisplay();
            }
        }
    }
    lastButtonState = reading;
}

// Función para escanear dispositivos I2C
bool i2c_isDeviceReady(uint8_t address)
{
  Wire.beginTransmission(address);
  return (Wire.endTransmission() == 0);
}
void i2c_scan() {
  Serial.println("--- Escaneando bus I2C ---");
  byte count = 0;
  for (uint8_t address = 1; address < 127; address++) {
    if (i2c_isDeviceReady(address)) {
      Serial.printf("[I2C] Dispositivo detectado en: 0x%02X", address);
      Serial.println();
      count++;
    }
  }
  if (count == 0) {
    Serial.println("[I2C] No se encontraron dispositivos. Revisa conexiones y alimentación.");
  } else {
    Serial.printf("[I2C] Escaneo finalizado. Total: %d", count);
  }
  Serial.println();
}



void setup() {
    // Inicializa el Serial Monitor
    Serial.begin(115200);    
    delay(100); 
    Serial.println("\n[SISTEMA] Iniciando Waveshare ESP32-S3-ETH...");

    // Inicializa el botón BOOT
    pinMode(BOOT_BUTTON, INPUT_PULLUP);


    // Inicializa los dispositivos I2C
    pinMode(I2C_SDA, INPUT_PULLUP);
    pinMode(I2C_SCL, INPUT_PULLUP);
    delay(10);
    if (!Wire.begin(I2C_SDA, I2C_SCL, 100000)) {
        Serial.println("[I2C] ERROR: No se pudo iniciar el bus Wire");
    } else {
        Serial.printf("[I2C] Bus iniciado en SDA:%d, SCL:%d a 100kHz", I2C_SDA, I2C_SCL);
    }
    Serial.println();    
    i2c_scan();   // Escanea el bus I2C para detectar dispositivos

    stLed.begin(); // Inicializa el LED de estado

    iconsInit();  // Inicializa el display y los iconos
    writeLine(0, "INICIANDO SISTEMA", true); 
    writeLine(1, "--------------------");

    // Inicializa los dispositivos conectados
    writeLine(2, "Tarjeta SD...");
    sdManager.begin(); 
    if(sdManager.isReady()) writeLine(2, "SD: LISTA OK      ");
    else writeLine(2, "SD: ERROR         ");    
    
    writeLine(3, "Ethernet.....");
    eth_begin(); // Comunicacion Ethernet vía W5500
    if (Ethernet.localIP()[0] != 0) writeLine(3, "ETH: CONECTADO");
    else writeLine(3, "ETH: SIN RED");
    
    writeLine(4, "RTC..........");
    if (rtcInit()) writeLine(4, "RTC: LISTO");
    else writeLine(4, "RTC: ERROR");

    // Inicializamos el sensor SHT45    
    writeLine(5, "SHT45........");
    if (shtManager.begin(&Wire)) writeLine(5, "SHT45: LISTO");
    else writeLine(5, "SHT45: ERROR");

    // Inicializamos el sensor BMI160    
    writeLine(6, "BMI160........");
    if (incline.begin()) writeLine(6, "BMI160: LISTO");
    else writeLine(6, "BMI160: ERROR");

    writeLine(7, "BH1750........");
    if (luxManager.begin()) writeLine(7, "BH1750: LISTO");
    else writeLine(7, "BH1750: ERROR");

    writeLine(8, "DS18B20.......");
    if (tempExt.begin()) writeLine(8, "DS18B20: LISTO");
    else writeLine(8, "DS18B20: ERROR");

    delay (1500); // Esperamos un momento para que el usuario pueda leer el estado de los dispositivos

    iconsInit();  // Presenta una segunda pantalla de inicio
    writeLine(0, "SISTEMA INICIADO", true); 
    writeLine(1, "--------------------");

    // Inicializa los dispositivos conectados
    writeLine(2, "Entradas/Salidas...");
    pcf.begin(0xFF00); // Configura todas las salidas en HIGH (apagadas) y entradas con pull-up
    if(pcf.isConnected()) writeLine(2, "I/O: LISTA OK");
    else writeLine(2, "I/O: ERROR");    

    writeLine(3, "SHT31.........");
    if (sht31Manager.begin(&Wire)) writeLine(3, "SHT31: LISTO");
    else writeLine(3, "SHT31: ERROR");

    writeLine(4, "INA3221 Potencia");
    if (inaBateria1.begin()) {
        writeLine(4, "INA3221.1: LISTO");
        inaBateria1.setChannelConfig(1, 0.01);      // Corriente Bat
        inaBateria1.setChannelConfig(2, 0.1, 2.0);  // Voltaje 12V (Aprox)
        inaBateria1.setChannelConfig(3, 0.1, 2.0);  // Voltaje 24V (Total)
    } else writeLine(4, "INA3221.1: ERROR");

    writeLine(5, "INA3221 Consumo A");
    if (inaConsumoA.begin()) {
        inaConsumoA.setChannelConfig(1, 0.1);      // Corriente 1
        inaConsumoA.setChannelConfig(2, 0.1);      // Corriente 2
        inaConsumoA.setChannelConfig(3, 0.1);      // Corriente 3
        writeLine(5, "INA3221.2: LISTO");
    } else writeLine(5, "INA3221.2: ERROR");

    writeLine(6, "INA3221 Consumo B");
    if (inaConsumoB.begin()) {
        inaConsumoB.setChannelConfig(1, 0.1);      // Corriente 4
        inaConsumoB.setChannelConfig(2, 0.1);      // Corriente 5
        inaConsumoB.setChannelConfig(3, 0.1);      // Corriente 6
        writeLine(6, "INA3221.3: LISTO");
    } else writeLine(6, "INA3221.3: ERROR");


    delay (1500); // Esperamos un momento para que el usuario pueda leer el estado de los dispositivos
    
    // intenta actualizar desde github
    buscarActualizacion();

    // Intentar descargar hora
    unsigned long epoch = get_ntp_time();
    if (epoch > 0) {
        Serial.printf("Hora sincronizada! Unix Time: %lu\n", epoch);
        updateRTCfromNTP(epoch); // Actualiza el DS3231 con la hora NTP y luego sincroniza el reloj interno
    } else {
        Serial.println("No se pudo obtener la hora NTP.");
    }
    if (sdManager.isReady()) {
        String bootMsg = "Sistema Iniciado - Epoch: " + String(epoch);
        sdManager.log("/boot.log", bootMsg.c_str());
    }

}

void loop() {
    // Lógica de control de 300W y lectura de sensores
    checkButton(); // Verifica el estado del botón para cambiar de página
    rtcUpdate();   // Actualiza sys.timeStr y gestiona la sincronización cada hora
    netUpdate();   // Gestión de red e iconos automática
    shtManager.update(); // Actualiza las lecturas del SHT45 y guarda en sys.tempInt y sys.humidity para la medida interior
    incline.update();    // Actualiza la inclinación del báculo y guarda en sys.tiltAngle
    luxManager.update();    // Actualiza la luminosidad exterior y guarda en sys.light
    sdManager.update(); // Verifica el estado de la tarjeta SD cada 5 segundos y actualiza sys.sdState
    stLed.update(); // Actualiza el LED de estado con animación suave
    sht31Manager.update(); // Actualiza las lecturas del SHT31 y guarda en sys.tempExt para la medida exterior
    pcf.update(); // Actualiza el estado de las entradas/salidas del PCF8575 y guarda en sys.pcfInputs/sys.pcfOutputs

    // Actualizar display cada 500ms
    if (millis() - lastDisplayUpdate >= displayInterval) {
        lastDisplayUpdate = millis();
        updateDisplay();
        actualizarMedidas();  // actualiza las medidas de los INA3221 y calcula potencias, energías y SOC

        if (pcf.isConnected()) {
            pcf.write_pin(0, pcf.read_pin(15)); 
            pcf.write_pin(1, pcf.read_pin(14)); 
            pcf.write_pin(2, pcf.read_pin(13)); 
        }


    }
}