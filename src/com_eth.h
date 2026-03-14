#ifndef COM_ETH_H
#define COM_ETH_H

#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include "icons.h"
#include "systemData.h"

extern SystemData sys;
unsigned long lastNetCheck = 0;
const unsigned long netCheckInterval = 3000; // Verificar 
unsigned long dataTimer = 0; 

// Variables globales para red
byte mac[6];
EthernetUDP Udp;
const char* ntpServer = "pool.ntp.org";
const unsigned int localPort = 8888;
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

/**
 * Inicializa el hardware Ethernet W5500
 */
void eth_begin() {
    // 1. Obtener la MAC única del ESP32 (Fábrica)
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    // 2. Reset físico del W5500
    pinMode(W5500_RST, OUTPUT);
    digitalWrite(W5500_RST, LOW);
    delay(100);
    digitalWrite(W5500_RST, HIGH);
    delay(200); 

    // 3. Inicializar SPI con los pines definidos en build_flags
    Serial.println("Configurando bus SPI...");
    SPI.begin(W5500_SCLK, W5500_MISO, W5500_MOSI, W5500_CS);
    
    // 4. Informar a la librería el pin CS
    Ethernet.init(W5500_CS);
    
    // 5. Iniciar Ethernet vía DHCP
    Serial.println("Iniciando Ethernet vía DHCP...");
    if (Ethernet.begin(mac, 5000, 1000) == 0) { 
        Serial.println("Fallo al configurar Ethernet usando DHCP");
        if (Ethernet.hardwareStatus() == EthernetNoHardware) {
            Serial.println("Error: No se encuentra el chip W5500.");
        } else if (Ethernet.linkStatus() == LinkOFF) {
            Serial.println("Error: Cable de red desconectado.");
        }
    } else {
        Serial.print("Ethernet OK! IP: ");
        Serial.println(Ethernet.localIP());
    }
}

void netUpdate() {
    unsigned long now = millis();

    // 1. Gestión del parpadeo de datos (vuelve de DATA a CONN tras 200ms)
    if (sys.lanIconIdx == LAN_DATA && (now - dataTimer > 200)) {
        sys.lanIconIdx = LAN_CONN;
    }

    if (now - lastNetCheck < netCheckInterval) return;
    lastNetCheck = now;

    // 2. Verificación de Hardware
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
        sys.lanIconIdx = LAN_ERR;
        return;
    }

    // 3. Verificación de Cable (Link)
    if (Ethernet.linkStatus() == LinkOFF) {
        sys.lanIconIdx = LAN_DISC;
        return;
    }

    // 4. Mantenimiento de IP (DHCP Lease)
    // Si hay actividad de mantenimiento, activamos el icono de DATA
    int maintainStatus = Ethernet.maintain();
    if (maintainStatus != 0) {
        sys.lanIconIdx = LAN_DATA;
        dataTimer = now;
    }

    // 5. Verificación de IP activa
    if (Ethernet.localIP()[0] == 0) {
        // Si tenemos cable pero no IP, intentamos re-conectar
        if (Ethernet.begin(mac, 500, 250) != 0) {
            sys.lanIconIdx = LAN_CONN;
        } else {
            sys.lanIconIdx = LAN_DISC;
        }
    } else {
        // Todo correcto, si no estamos en DATA, aseguramos CONN
        if (sys.lanIconIdx != LAN_DATA) sys.lanIconIdx = LAN_CONN;
    }

    if (Ethernet.linkStatus() == LinkON && Ethernet.localIP()[0] != 0) {
        sys.lanIconIdx = LAN_CONN; // Usamos lanIconIdx que es el que maneja el Manager
        
        // VOLCADO DE DATOS A SYS (Importante)
        // Convertimos los objetos IPAddress a Strings para que tu display los lea
        sys.ipAddress = Ethernet.localIP().toString();
        sys.gateway = Ethernet.gatewayIP().toString();
        sys.dns = Ethernet.dnsServerIP().toString();
        sys.subnetMask = Ethernet.subnetMask().toString();
    } else {
        sys.lanIconIdx = LAN_DISC;
    }
}

/**
 * Descarga la hora actual desde un servidor NTP
 * Retorna Unix Timestamp (segundos desde 1970)
 */
unsigned long get_ntp_time() {
    if (Ethernet.linkStatus() == LinkOFF) return 0;

    Udp.begin(localPort);
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    
    // Preparar el paquete NTP (LI, Version, Mode)
    packetBuffer[0] = 0b11100011;   

    if (!Udp.beginPacket(ntpServer, 123)) return 0;
    Udp.write(packetBuffer, NTP_PACKET_SIZE);
    Udp.endPacket();

    // Esperar respuesta (max 1s)
    uint32_t startTime = millis();
    while (Udp.parsePacket() == 0) {
        if (millis() - startTime > 1000) {
            Udp.stop();
            return 0;
        }
    }

    Udp.read(packetBuffer, NTP_PACKET_SIZE);
    
    // Extraer segundos (bytes 40 a 43)
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    unsigned long secsSince1900 = highWord << 16 | lowWord;

    // Convertir a Unix Epoch (1900 -> 1970 = 2208988800 segundos)
    const unsigned long seventyYears = 2208988800UL;
    Udp.stop();
    
    return secsSince1900 - seventyYears;
}

#endif