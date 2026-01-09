#pragma once
#include <Arduino.h>
#include <Ethernet.h>

// IDs conocidos (puedes añadir más aquí)
constexpr uint16_t WEIGHT_ID = 85; 
constexpr uint16_t TARE_ID   = 113;

struct WeightReading {
    bool ok;
    float kg;
};

class MarelClient {
public:
    MarelClient(const char* serverIp, uint16_t serverPort, byte mac[], IPAddress ip, IPAddress gateway, IPAddress subnet, uint8_t csPin = 10);

    // Inicializa la Ethernet
    void begin();

    // Conectar al servidor Marel M2200
    bool connectToServer();

    // ¿Está conectado?
    bool isConnected();

    // Función genérica para enviar cualquier comando
    String sendCommand(const String& cmd);

    // Leer un valor del Modelo: .R<ID>:<dimension>
    String readValue(uint16_t modelID, uint8_t dimension);

    void setTare();

    void setZero();

    String getWeight();

    float getWeightKg();

    float parseWeightKg(const String& response);

    String getTare();

    // Escribir un valor en el Modelo: .W<ID>:<dimension>:<valor>
    String writeValue(uint16_t modelID, uint8_t dimension, const String& value);

private:
    const char* _serverIp;
    uint16_t _serverPort;
    uint8_t _csPin;
    byte* _mac;
    IPAddress _ip, _gateway, _subnet;
    EthernetClient _client;

    // Enviar un comando y esperar respuesta
    String castWeightResponse(const String& response);
};
