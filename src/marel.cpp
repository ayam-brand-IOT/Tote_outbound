#include "marel.h"
#include <SPI.h>

MarelClient::MarelClient(const char* serverIp, uint16_t serverPort, byte mac[], IPAddress ip, IPAddress gateway, IPAddress subnet, uint8_t csPin)
: _serverIp(serverIp), _serverPort(serverPort), _csPin(csPin), _mac(mac), _ip(ip), _gateway(gateway), _subnet(subnet) {}

void MarelClient::begin() {
    Ethernet.init(_csPin);
    Ethernet.begin(_mac, _ip, _gateway, _subnet);
    _client.setTimeout(5000); 
}

bool MarelClient::connectToServer() {
    if (!_client.connected()) {
        if (!_client.connect(_serverIp, _serverPort)) {
            return false;
        }
    }
    return true;
}

bool MarelClient::isConnected() {
    return _client.connected();
}

String MarelClient::sendCommand(const String& cmd) {
    if (!isConnected()) {
        _client.stop();
        if (!connectToServer()) {
            return "ERROR: No connection";
        }
    }

    _client.print(cmd);

    // Esperar respuesta
    unsigned long start = millis();
    String response;
    while ((millis() - start) < 2000 && !_client.available()) {
        delay(10);
    }

    while (_client.available()) {
        response += (char)_client.read();
    }

    if (response.length() == 0) {
        _client.stop();
    }

    response.trim();
    return response;
}

String MarelClient::readValue(uint16_t modelID, uint8_t dimension) {
    // Ejemplo: .R100:2\n
    String cmd = ".R" + String(modelID) + ":" + String(dimension) + "\n";
    String response = sendCommand(cmd);
    // example of response .D.85.2:-2.00kg"
    int start = response.indexOf(":") + 1;
    int end = response.indexOf("kg");
    return response.substring(start, end);
}

String MarelClient::writeValue(uint16_t modelID, uint8_t dimension, const String& value) {
    // Ejemplo: .W100:2:123\n
    String cmd = ".W" + String(modelID) + ":" + value + "\n";
    Serial.println("===============> Command to write: " + cmd);
    return sendCommand(cmd);
}

void MarelClient::setTare(){
    const String weightValue = getWeight();

    writeValue(TARE_ID, 2, weightValue);
}

void MarelClient::setZero(){
    // writeValue(TARE_ID, 2, "0");
}

String MarelClient::getWeight(){
    String response = readValue(WEIGHT_ID, 2);
    if (response.length() == 0) {
        _client.stop();
        if (connectToServer()) {
            response = readValue(WEIGHT_ID, 2);
        }
    }
    return castWeightResponse(response);
}

float MarelClient::getWeightKg(){
    String response = readValue(WEIGHT_ID, 2);

    if (response.length() == 0) {
        _client.stop();
        if (!connectToServer()) {
            return NAN;
        }
        response = readValue(WEIGHT_ID, 2);
        if (response.length() == 0) {
            return NAN;
        }
    }
    return parseWeightKg(response);  // nueva función numérica
}

String MarelClient::getTare(){
    String response = readValue(TARE_ID, 2);

    return castWeightResponse(response);
}

String MarelClient::castWeightResponse(const String& response) {
    // example of response .D.85.2:-2.00kg"
    int start = response.indexOf(":") + 1;
    int end = response.indexOf("kg");
    return response.substring(start, end);
}

float MarelClient::parseWeightKg(const String& resp) {
    int colon = resp.lastIndexOf(':');
    if (colon < 0) return NAN;

    String num = resp.substring(colon + 1);
    num.replace("kg", "");
    num.trim();
    if (num.length() == 0) return NAN;

    return num.toFloat();  // kg
}

