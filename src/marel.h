#pragma once
#include <Arduino.h>
#include <ModbusRTU.h>

// Modbus addresses according to simulator
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
    // Constructor for Modbus RTU
    MarelClient(uint8_t slaveID, uint8_t rxPin, uint8_t txPin, uint8_t dePin);

    // Initialize Modbus RTU
    void begin();

    // Process Modbus tasks (call in loop)
    void task();

    // Is connected? (for Modbus RTU always returns true if initialized)
    bool isConnected();

    // Read gross weight in kg
    float getWeightKg();

    // Read net weight in kg
    float getNetWeightKg();

    // Read tare value in kg
    float getTareKg();

    // Set tare (writes current weight as tare)
    bool setTare();

    // Clear tare
    bool clearTare();

    // Execute Zero command
    bool setZero();

    // Check if weight is stable
    bool isWeightStable();

    // Callback for read response
    bool cbRead(Modbus::ResultCode event, uint16_t transactionId, void* data);

private:
    ModbusRTU _mb;
    uint8_t _slaveID;
    uint8_t _rxPin;
    uint8_t _txPin;
    uint8_t _dePin;
    bool _initialized;

    // Buffers for readings
    uint16_t _weightRegs[2];
    uint16_t _netWeightRegs[2];
    uint16_t _tareRegs[2];
    bool _readComplete;
    
    // Convert two uint16_t registers to float
    float registersToFloat(uint16_t low, uint16_t high);
    
    // Convert float to two uint16_t registers
    void floatToRegisters(float value, uint16_t &low, uint16_t &high);
};
