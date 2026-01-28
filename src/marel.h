#pragma once
#include <Arduino.h>
#include <ModbusRTU.h>

// Direcciones Modbus según el simulador
// Holding Registers (40001-41020)
#define REG_DISPLAY_WEIGHT_LOW      0      // 40001
#define REG_DISPLAY_WEIGHT_HIGH     1      // 40002
#define REG_GROSS_WEIGHT_LOW        2      // 40003
#define REG_GROSS_WEIGHT_HIGH       3      // 40004
#define REG_NET_WEIGHT_LOW          4      // 40005
#define REG_NET_WEIGHT_HIGH         5      // 40006
#define REG_TARE_VALUE_LOW          6      // 40007
#define REG_TARE_VALUE_HIGH         7      // 40008

// Coils (00001-01016)
#define COIL_WEIGHT_STABLE          1      // 00002
#define COIL_ZERO                   1000   // 01001
#define COIL_TARE                   1002   // 01003
#define COIL_CLEAR_TARE             1003   // 01004
struct WeightReading {
    bool ok;
    float kg;
};

class MarelClient {
public:
    // Constructor para Modbus RTU
    MarelClient(uint8_t slaveID, uint8_t rxPin, uint8_t txPin, uint8_t dePin);

    // Inicializa el Modbus RTU
    void begin();

    // Procesar tareas Modbus (llamar en loop)
    void task();

    // ¿Está conectado? (para Modbus RTU siempre devuelve true si está inicializado)
    bool isConnected();

    // Leer peso bruto en kg
    float getWeightKg();

    // Leer peso neto en kg
    float getNetWeightKg();

    // Leer valor de tara en kg
    float getTareKg();

    // Establecer tara (escribe peso actual como tara)
    bool setTare();

    // Limpiar tara
    bool clearTare();

    // Ejecutar comando Zero
    bool setZero();

    // Verificar si el peso está estable
    bool isWeightStable();

    // Callback para respuesta de lectura
    bool cbRead(Modbus::ResultCode event, uint16_t transactionId, void* data);

private:
    ModbusRTU _mb;
    uint8_t _slaveID;
    uint8_t _rxPin;
    uint8_t _txPin;
    uint8_t _dePin;
    bool _initialized;

    // Buffers para lecturas
    uint16_t _weightRegs[2];
    uint16_t _netWeightRegs[2];
    uint16_t _tareRegs[2];
    bool _readComplete;
    
    // Convierte dos registros uint16_t a float
    float registersToFloat(uint16_t low, uint16_t high);
    
    // Convierte float a dos registros uint16_t
    void floatToRegisters(float value, uint16_t &low, uint16_t &high);
};
