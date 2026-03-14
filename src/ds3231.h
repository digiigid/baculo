#ifndef DS3231_MANAGER_H
#define DS3231_MANAGER_H

#include <Wire.h>
#include <RTClib.h>
#include <time.h>
#include "systemData.h"

RTC_DS3231 rtc;
extern SystemData sys;

// Configuración de sincronización (1 hora = 3600000 ms)
unsigned long lastSyncMillis = 0;
const unsigned long syncInterval = 3600000; 
const char* TZ_INFO = "CET-1CEST,M3.5.0,M10.5.0/3";

/**
 * Carga la hora del DS3231 al reloj interno del ESP32
 */
void syncInternalFromRTC() {
    if (!rtc.begin()) return;
    
    DateTime now = rtc.now();
    struct timeval tv;
    tv.tv_sec = now.unixtime();
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);

    setenv("TZ", TZ_INFO, 1);     // Aplicar la zona horaria de España
    tzset();

    Serial.println(F("[RTC] Reloj interno sincronizado desde DS3231"));
}

/**
 * Inicializa el DS3231 y carga el tiempo inicial
 */
bool rtcInit() {
    if (!rtc.begin()) {
        Serial.println(F("[RTC] ERROR: No se encontró DS3231"));
        return false;
    }

    if (rtc.lostPower()) {
        Serial.println(F("[RTC] Batería agotada, configurando fecha compilación..."));
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    syncInternalFromRTC();
    return true;
}

/**
 * Actualiza el DS3231 con un valor UNIX (desde NTP) 
 * y luego actualiza el reloj interno
 */
void updateRTCfromNTP(unsigned long epoch) {
    if (epoch == 0) return;
    
    rtc.adjust(DateTime(epoch));
    syncInternalFromRTC();
    Serial.println(F("[RTC] DS3231 e Interno actualizados vía NTP"));
}

/**
 * Actualiza los strings de sistema y gestiona la sincronización horaria
 */
void rtcUpdate() {
    // 1. Sincronización automática cada hora
    if (millis() - lastSyncMillis >= syncInterval) {
        lastSyncMillis = millis();
        syncInternalFromRTC();
    }

    // 2. Obtener tiempo actual del RTC interno del ESP32
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // 3. Formatear sys.timeStr (HH:MM:SS)
    strftime(sys.timeStr, sizeof(sys.timeStr), "%H:%M:%S", &timeinfo);

    // 4. Formatear sys.dateStr (DD/MM/YYYY)
    strftime(sys.dateStr, sizeof(sys.dateStr), "%d/%m/%Y", &timeinfo);
}

#endif