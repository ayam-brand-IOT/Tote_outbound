#include "marel.h"

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
    // Configurar Serial1 para Modbus RTU en los pines especificados
    Serial1.begin(9600, SERIAL_8N1, _rxPin, _txPin);
    
    // Configurar pin DE/RE
    pinMode(_dePin, OUTPUT);
    digitalWrite(_dePin, LOW);  // Modo recepción por defecto
    
    // Inicializar Modbus como Master
    _mb.begin(&Serial1, _dePin);
    _mb.master();
    
    _initialized = true;
    
    Serial.println("Modbus RTU Master initialized");
    Serial.printf("Slave ID: %d, RX: GPIO%d, TX: GPIO%d, DE/RE: GPIO%d\n", 
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

float MarelClient::registersToFloat(uint16_t low, uint16_t high) {
    int32_t intValue = ((int32_t)high << 16) | low;
    return intValue / 100.0;
}

void MarelClient::floatToRegisters(float value, uint16_t &low, uint16_t &high) {
    int32_t intValue = (int32_t)(value * 100.0);
    low = intValue & 0xFFFF;
    high = (intValue >> 16) & 0xFFFF;
}

float MarelClient::getWeightKg() {
    if (!_initialized) return 0.0;
    
    // Leer registros 2-3 (Gross Weight)
    if (!_mb.readHreg(_slaveID, REG_GROSS_WEIGHT_LOW, _weightRegs, 2)) {
        Serial.println("Error: Failed to queue read request for weight");
        return 0.0;
    }
    
    // Esperar respuesta (timeout 1 segundo)
    unsigned long start = millis();
    while (_mb.slave() && (millis() - start) < 1000) {
        _mb.task();
        yield();
    }
    
    float weight = registersToFloat(_weightRegs[0], _weightRegs[1]);
    Serial.printf("Modbus Read: Regs[%04X, %04X] = %.2f kg\n", _weightRegs[0], _weightRegs[1], weight);
    return weight;
}

float MarelClient::getNetWeightKg() {
    if (!_initialized) return 0.0;
    
    // Leer registros 4-5 (Net Weight)
    if (!_mb.readHreg(_slaveID, REG_NET_WEIGHT_LOW, _netWeightRegs, 2)) {
        Serial.println("Error: Failed to queue read request for net weight");
        return 0.0;
    }
    
    // Esperar respuesta
    unsigned long start = millis();
    while (_mb.slave() && (millis() - start) < 1000) {
        _mb.task();
        yield();
    }
    
    float netWeight = registersToFloat(_netWeightRegs[0], _netWeightRegs[1]);
    Serial.printf("Modbus Read NET: Regs[%04X, %04X] = %.2f kg\n", _netWeightRegs[0], _netWeightRegs[1], netWeight);
    return netWeight;
}

float MarelClient::getTareKg() {
    if (!_initialized) return 0.0;
    
    // Leer registros 6-7 (Tare Value)
    if (!_mb.readHreg(_slaveID, REG_TARE_VALUE_LOW, _tareRegs, 2)) {
        Serial.println("Error: Failed to queue read request for tare");
        return 0.0;
    }
    
    // Esperar respuesta
    unsigned long start = millis();
    while (_mb.slave() && (millis() - start) < 1000) {
        _mb.task();
        yield();
    }
    
    return registersToFloat(_tareRegs[0], _tareRegs[1]);
}

bool MarelClient::setTare() {
    if (!_initialized) return false;
    
    Serial.println("Executing TARE command...");
    
    // Escribir coil 1002 (COIL_TARE) = true
    if (!_mb.writeCoil(_slaveID, COIL_TARE, true)) {
        Serial.println("Error: Failed to queue TARE command");
        return false;
    }
    
    // Esperar confirmación
    unsigned long start = millis();
    while (_mb.slave() && (millis() - start) < 1000) {
        _mb.task();
        yield();
    }
    
    Serial.println("TARE command sent successfully");
    return true;
}

bool MarelClient::clearTare() {
    if (!_initialized) return false;
    
    Serial.println("Executing CLEAR TARE command...");
    
    // Escribir coil 1003 (COIL_CLEAR_TARE) = true
    if (!_mb.writeCoil(_slaveID, COIL_CLEAR_TARE, true)) {
        Serial.println("Error: Failed to queue CLEAR TARE command");
        return false;
    }
    
    // Esperar confirmación
    unsigned long start = millis();
    while (_mb.slave() && (millis() - start) < 1000) {
        _mb.task();
        yield();
    }
    
    Serial.println("CLEAR TARE command sent successfully");
    return true;
}

bool MarelClient::setZero() {
    if (!_initialized) return false;
    
    Serial.println("Executing ZERO command...");
    
    // Escribir coil 1000 (COIL_ZERO) = true
    if (!_mb.writeCoil(_slaveID, COIL_ZERO, true)) {
        Serial.println("Error: Failed to queue ZERO command");
        return false;
    }
    
    // Esperar confirmación
    unsigned long start = millis();
    while (_mb.slave() && (millis() - start) < 1000) {
        _mb.task();
        yield();
    }
    
    Serial.println("ZERO command sent successfully");
    return true;
}

bool MarelClient::isWeightStable() {
    if (!_initialized) return false;
    
    bool stable = false;
    
    // Leer coil 1 (COIL_WEIGHT_STABLE)
    if (!_mb.readCoil(_slaveID, COIL_WEIGHT_STABLE, &stable, 1)) {
        Serial.println("Error: Failed to queue read for weight stable flag");
        return false;
    }
    
    // Esperar respuesta
    unsigned long start = millis();
    while (_mb.slave() && (millis() - start) < 1000) {
        _mb.task();
        yield();
    }
    
    return stable;
}

bool MarelClient::cbRead(Modbus::ResultCode event, uint16_t transactionId, void* data) {
    if (event != Modbus::EX_SUCCESS) {
        Serial.printf("Modbus read error: 0x%02X\n", event);
        return false;
    }
    _readComplete = true;
    return true;
}

