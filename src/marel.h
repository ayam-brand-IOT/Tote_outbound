#pragma once
#include <Arduino.h>
#include <ModbusRTU.h>

// Modbus addresses - Marel M2200 Holding Registers (base address, reads 2 regs)
// Word order: ABCD big-endian → addr N = HIGH word, addr N+1 = LOW word
// Value encoding: int32 / 100 = kg
#define REG_DISPLAY_WEIGHT      0      // 40001 (H) + 40002 (L)
#define REG_GROSS_WEIGHT        2      // 40003 (H) + 40004 (L)
#define REG_NET_WEIGHT          4      // 40005 (H) + 40006 (L)
#define REG_TARE_VALUE          6      // 40007 (H) + 40008 (L)

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
    
    float registersToFloat(uint16_t reg0, uint16_t reg1);
    void  floatToRegisters(float value, uint16_t &reg0, uint16_t &reg1);
};
