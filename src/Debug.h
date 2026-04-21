#pragma once
// ================================================================
// Debug.h — Configuración centralizada de debug por módulo
//
// Cambia el valor a 1 para activar o 0 para desactivar el debug
// de cada módulo individualmente. Los errores críticos (LOG_ERR)
// siempre se muestran sin importar el valor de los flags.
// ================================================================

// ── Toggles por módulo ──────────────────────────────────────────
#define DEBUG_MAIN    1   // main.cpp      – máquina de estados, bombas
#define DEBUG_MAREL   0   // marel.cpp     – Modbus RTU registros/coils
#define DEBUG_WS      0   // websocket_client.cpp – eventos WS (muy ruidoso)
#define DEBUG_WIFI    0   // WIFI.cpp      – servidor HTTP / OTA / reconexión
#define DEBUG_CTRL    0   // Controller.cpp – peso, I/O, estado interno
#define DEBUG_BLE     1   // BLEQRClient   – BLE scan/connect/QR

// ── Macros por módulo ────────────────────────────────────────────
// Uso:  LOG_MAIN("Mensaje\n")
//       LOG_MAIN("Peso: %.2f kg\n", val)
#define LOG_MAIN(fmt, ...)   do { if (DEBUG_MAIN)  Serial.printf(fmt, ##__VA_ARGS__); } while(0)
#define LOG_MAREL(fmt, ...)  do { if (DEBUG_MAREL) Serial.printf(fmt, ##__VA_ARGS__); } while(0)
#define LOG_WS(fmt, ...)     do { if (DEBUG_WS)    Serial.printf(fmt, ##__VA_ARGS__); } while(0)
#define LOG_WIFI(fmt, ...)   do { if (DEBUG_WIFI)  Serial.printf(fmt, ##__VA_ARGS__); } while(0)
#define LOG_CTRL(fmt, ...)   do { if (DEBUG_CTRL)  Serial.printf(fmt, ##__VA_ARGS__); } while(0)
#define LOG_BLE(fmt, ...)    do { if (DEBUG_BLE)   Serial.printf(fmt, ##__VA_ARGS__); } while(0)

// Siempre visible — errores críticos (no se puede desactivar)
#define LOG_ERR(fmt, ...)    Serial.printf("[ERR] " fmt, ##__VA_ARGS__)
