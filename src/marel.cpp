#include "marel.h"
#include "Debug.h"

MarelClient::MarelClient(uint8_t slaveID, uint8_t rxPin, uint8_t txPin, uint8_t dePin)
    : _slaveID(slaveID), _rxPin(rxPin), _txPin(txPin), _dePin(dePin), _initialized(false), _readComplete(false) {
    _weightRegs[0] = 0;
    _weightRegs[1] = 0;
    _netWeightRegs[0] = 0;
    _netWeightRegs[1] = 0;
    _tareRegs[0] = 0;
    _tareRegs[1] = 0;
}

void MarelClient::begin() {
    // Configure Serial1 for Modbus RTU on specified pins
    Serial1.begin(9600, SERIAL_8N1, _rxPin, _txPin);
    
    // Configure DE/RE pin
    pinMode(_dePin, OUTPUT);
    digitalWrite(_dePin, LOW);  // Reception mode by default
    
    // Initialize Modbus as Master
    _mb.begin(&Serial1, _dePin);
    _mb.master();
    
    _initialized = true;
    
    LOG_MAREL("Modbus RTU Master initialized\n");
    LOG_MAREL("Slave ID: %d, RX: GPIO%d, TX: GPIO%d, DE/RE: GPIO%d\n", 
                  _slaveID, _rxPin, _txPin, _dePin);
}

void MarelClient::task() {
    if (_initialized) {
        _mb.task();
    }
}

bool MarelClient::isConnected() {
    return _initialized;
}

float MarelClient::registersToFloat(uint16_t reg0, uint16_t reg1) {
    // CDAB word order - usar uint32_t como intermediario para preservar sign two's complement
    uint32_t uraw = ((uint32_t)reg1 << 16) | (uint32_t)reg0;
    int32_t  raw  = (int32_t)uraw;          // reinterpret como signed
    float result  = (float)raw;
    LOG_MAREL("  [Marel] Regs[%04X, %04X] raw=%ld → %.2f kg\n", reg0, reg1, raw, result);
    return result;
}

void MarelClient::floatToRegisters(float value, uint16_t &reg0, uint16_t &reg1) {
    int32_t raw = (int32_t)value;  // raw IS kg, no scaling
    reg0 =  raw        & 0xFFFF;   // CDAB: reg0 = LOW word
    reg1 = (raw >> 16) & 0xFFFF;   // CDAB: reg1 = HIGH word
}

float MarelClient::getWeightKg() {
    if (!_initialized) return 0.0;
    
    // Read registers 2-3 (Gross Weight)
    if (!_mb.readHreg(_slaveID, REG_GROSS_WEIGHT, _weightRegs, 2)) {
        LOG_ERR("Failed to queue read request for weight\n");
        return 0.0;
    }
    
    // Wait for response (1 second timeout)
    unsigned long start = millis();
    while (_mb.slave() && (millis() - start) < 1000) {
        _mb.task();
        yield();
    }
    
    float weight = registersToFloat(_weightRegs[0], _weightRegs[1]);
    LOG_MAREL("Modbus Read: Regs[%04X, %04X] = %.2f kg\n", _weightRegs[0], _weightRegs[1], weight);
    return weight;
}

float MarelClient::getNetWeightKg() {
    if (!_initialized) return 0.0;
    
    // Read registers 4-5 (Net Weight)
    if (!_mb.readHreg(_slaveID, REG_NET_WEIGHT, _netWeightRegs, 2)) {
        LOG_ERR("Failed to queue read request for net weight\n");
        return 0.0;
    }
    
    // Wait for response
    unsigned long start = millis();
    while (_mb.slave() && (millis() - start) < 1000) {
        _mb.task();
        yield();
    }
    
    float netWeight = registersToFloat(_netWeightRegs[0], _netWeightRegs[1]);
    LOG_MAREL("Modbus Read NET: Regs[%04X, %04X] = %.2f kg\n", _netWeightRegs[0], _netWeightRegs[1], netWeight);
    return netWeight;
}

float MarelClient::getTareKg() {
    if (!_initialized) return 0.0;
    
    // Read registers 6-7 (Tare Value)
    if (!_mb.readHreg(_slaveID, REG_TARE_VALUE, _tareRegs, 2)) {
        LOG_ERR("Failed to queue read request for tare\n");
        return 0.0;
    }
    
    // Wait for response
    unsigned long start = millis();
    while (_mb.slave() && (millis() - start) < 1000) {
        _mb.task();
        yield();
    }
    
    return registersToFloat(_tareRegs[0], _tareRegs[1]);
}

bool MarelClient::setTare() {
    if (!_initialized) return false;
    
    LOG_MAREL("Executing TARE command...\n");
    
    // Write coil 1002 (COIL_TARE) = true
    if (!_mb.writeCoil(_slaveID, COIL_TARE, true)) {
        LOG_ERR("Failed to queue TARE command\n");
        return false;
    }
    
    // Wait for confirmation
    unsigned long start = millis();
    while (_mb.slave() && (millis() - start) < 1000) {
        _mb.task();
        yield();
    }
    
    LOG_MAREL("TARE command sent successfully\n");
    return true;
}

bool MarelClient::clearTare() {
    if (!_initialized) return false;
    
    LOG_MAREL("Executing CLEAR TARE command...\n");
    
    // Write coil 1003 (COIL_CLEAR_TARE) = true
    if (!_mb.writeCoil(_slaveID, COIL_CLEAR_TARE, true)) {
        LOG_ERR("Failed to queue CLEAR TARE command\n");
        return false;
    }
    
    // Wait for confirmation
    unsigned long start = millis();
    while (_mb.slave() && (millis() - start) < 1000) {
        _mb.task();
        yield();
    }
    
    LOG_MAREL("CLEAR TARE command sent successfully\n");
    return true;
}

bool MarelClient::setZero() {
    if (!_initialized) return false;
    
    LOG_MAREL("Executing ZERO command...\n");
    
    // Write coil 1000 (COIL_ZERO) = true
    if (!_mb.writeCoil(_slaveID, COIL_ZERO, true)) {
        LOG_ERR("Failed to queue ZERO command\n");
        return false;
    }
    
    // Wait for confirmation
    unsigned long start = millis();
    while (_mb.slave() && (millis() - start) < 1000) {
        _mb.task();
        yield();
    }
    
    LOG_MAREL("ZERO command sent successfully\n");
    return true;
}

bool MarelClient::isWeightStable() {
    if (!_initialized) return false;
    
    bool stable = false;
    
    // Read coil 1 (COIL_WEIGHT_STABLE)
    if (!_mb.readCoil(_slaveID, COIL_WEIGHT_STABLE, &stable, 1)) {
        LOG_ERR("Failed to queue read for weight stable flag\n");
        return false;
    }
    
    // Wait for response
    unsigned long start = millis();
    while (_mb.slave() && (millis() - start) < 1000) {
        _mb.task();
        yield();
    }
    
    return stable;
}

bool MarelClient::cbRead(Modbus::ResultCode event, uint16_t transactionId, void* data) {
    if (event != Modbus::EX_SUCCESS) {
        LOG_ERR("Modbus read error: 0x%02X\n", event);
        return false;
    }
    _readComplete = true;
    return true;
}

