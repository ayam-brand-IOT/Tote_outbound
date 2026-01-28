#include "websocket_client.h"

// Static instance pointer for callback
ToteWebSocketClient* ToteWebSocketClient::instance = nullptr;

ToteWebSocketClient::ToteWebSocketClient() 
    : isConnected(false)
    , lastHeartbeat(0)
    , reconnectDelay(1000)
    , lastReconnectAttempt(0)
    , reconnectAttempts(0)
    , messageCallback(nullptr) {
    instance = this;
}

bool ToteWebSocketClient::begin(const char* host, uint16_t port, const char* path) {
    Serial.printf("Connecting to WebSocket: ws://%s:%d%s\n", host, port, path);
    
    webSocket.begin(host, port, path);
    webSocket.onEvent(webSocketEventStatic);
    webSocket.setReconnectInterval(5000);
    
    return true;
}

void ToteWebSocketClient::loop() {
    webSocket.loop();
    
    // Send periodic heartbeat
    if (isConnected && (millis() - lastHeartbeat > HEARTBEAT_INTERVAL)) {
        sendHeartbeat();
        lastHeartbeat = millis();
    }
}

void ToteWebSocketClient::webSocketEventStatic(WStype_t type, uint8_t * payload, size_t length) {
    if (instance) {
        instance->webSocketEvent(type, payload, length);
    }
}

void ToteWebSocketClient::webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.println("[WS] Disconnected");
            isConnected = false;
            break;
            
        case WStype_CONNECTED: {
            Serial.printf("[WS] Connected to url: %s\n", payload);
            isConnected = true;
            reconnectAttempts = 0;
            reconnectDelay = 1000;
            lastHeartbeat = millis();
            
            // Identify as ESP32 client
            StaticJsonDocument<128> doc;
            doc["type"] = "identify";
            doc["clientType"] = "esp32";
            
            String output;
            serializeJson(doc, output);
            webSocket.sendTXT(output);
            break;
        }
            
        case WStype_TEXT:
            Serial.printf("[WS] Received: %s\n", payload);
            handleMessage(String((char*)payload));
            break;
            
        case WStype_ERROR:
            Serial.println("[WS] Error");
            break;
            
        case WStype_PING:
            Serial.println("[WS] Ping received");
            break;
            
        case WStype_PONG:
            Serial.println("[WS] Pong received");
            break;
    }
}

void ToteWebSocketClient::handleMessage(String payload) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        Serial.print("[WS] JSON parse error: ");
        Serial.println(error.c_str());
        return;
    }
    
    const char* type = doc["type"];
    if (!type) {
        Serial.println("[WS] Message missing 'type' field");
        return;
    }
    
    Serial.printf("[WS] Message type: %s\n", type);
    
    // Handle heartbeat response
    if (strcmp(type, "heartbeat") == 0) {
        Serial.println("[WS] Heartbeat acknowledged");
        return;
    }
    
    // Handle command messages
    if (strcmp(type, "command") == 0) {
        const char* command = doc["command"];
        Serial.printf("[WS] Command received: %s\n", command);
        
        // Call user callback if set
        if (messageCallback) {
            messageCallback(String(type), doc);
        }
        return;
    }
    
    // Handle qr_scanned from browser
    if (strcmp(type, "qr_scanned") == 0) {
        const char* toteId = doc["toteId"];
        Serial.printf("[WS] QR scanned from browser: %s\n", toteId);
        
        // Call user callback if set
        if (messageCallback) {
            messageCallback(String(type), doc);
        }
        return;
    }
    
    // Call user callback for other message types
    if (messageCallback) {
        messageCallback(String(type), doc);
    }
}

void ToteWebSocketClient::sendHeartbeat() {
    if (!isConnected) return;
    
    StaticJsonDocument<128> doc;
    doc["type"] = "heartbeat";
    doc["station"] = "outbound";
    
    String output;
    serializeJson(doc, output);
    webSocket.sendTXT(output);
}

bool ToteWebSocketClient::sendWeight(float weight) {
    if (!isConnected) {
        Serial.println("[WS] Not connected, cannot send weight");
        return false;
    }
    
    StaticJsonDocument<128> doc;
    doc["type"] = "weight_update";
    doc["station"] = "outbound";
    doc["weight"] = weight;
    
    String output;
    serializeJson(doc, output);
    
    webSocket.sendTXT(output);
    Serial.printf("[WS] Sent weight: %.2f kg\n", weight);
    
    return true;
}

bool ToteWebSocketClient::sendStateChange(const char* state) {
    if (!isConnected) {
        Serial.println("[WS] Not connected, cannot send state");
        return false;
    }
    
    StaticJsonDocument<128> doc;
    doc["type"] = "state_change";
    doc["station"] = "outbound";
    doc["state"] = state;
    
    String output;
    serializeJson(doc, output);
    
    webSocket.sendTXT(output);
    Serial.printf("[WS] Sent state: %s\n", state);
    
    return true;
}

bool ToteWebSocketClient::sendToteValidated(const char* toteId) {
    if (!isConnected) {
        Serial.println("[WS] Not connected, cannot send validation");
        return false;
    }
    
    StaticJsonDocument<128> doc;
    doc["type"] = "tote_validated";
    doc["station"] = "outbound";
    doc["toteId"] = toteId;
    
    String output;
    serializeJson(doc, output);
    
    webSocket.sendTXT(output);
    Serial.printf("[WS] Tote validated: %s\n", toteId);
    
    return true;
}

bool ToteWebSocketClient::sendToteCompleted(const char* toteId) {
    if (!isConnected) {
        Serial.println("[WS] Not connected, cannot send completion");
        return false;
    }
    
    StaticJsonDocument<128> doc;
    doc["type"] = "tote_completed";
    doc["station"] = "outbound";
    doc["toteId"] = toteId;
    
    String output;
    serializeJson(doc, output);
    
    webSocket.sendTXT(output);
    Serial.printf("[WS] Tote completed: %s\n", toteId);
    
    return true;
}

bool ToteWebSocketClient::sendIceDispensed(float ice_kg) {
    if (!isConnected) {
        Serial.println("[WS] Not connected, cannot send ice dispensed");
        return false;
    }
    
    StaticJsonDocument<128> doc;
    doc["type"] = "ice_dispensed";
    doc["station"] = "outbound";
    doc["ice_kg"] = ice_kg;
    
    String output;
    serializeJson(doc, output);
    
    webSocket.sendTXT(output);
    Serial.printf("[WS] Ice dispensed: %.2f kg\n", ice_kg);
    
    return true;
}

bool ToteWebSocketClient::sendWaterDispensed(float water_kg) {
    if (!isConnected) {
        Serial.println("[WS] Not connected, cannot send water dispensed");
        return false;
    }
    
    StaticJsonDocument<128> doc;
    doc["type"] = "water_dispensed";
    doc["station"] = "outbound";
    doc["water_kg"] = water_kg;
    
    String output;
    serializeJson(doc, output);
    
    webSocket.sendTXT(output);
    Serial.printf("[WS] Water dispensed: %.2f kg\n", water_kg);
    
    return true;
}

bool ToteWebSocketClient::sendError(const char* message) {
    if (!isConnected) {
        Serial.println("[WS] Not connected, cannot send error");
        return false;
    }
    
    StaticJsonDocument<256> doc;
    doc["type"] = "error";
    doc["station"] = "outbound";
    doc["message"] = message;
    
    String output;
    serializeJson(doc, output);
    
    webSocket.sendTXT(output);
    Serial.printf("[WS] Error sent: %s\n", message);
    
    return true;
}

void ToteWebSocketClient::setMessageCallback(void (*callback)(String type, JsonDocument& doc)) {
    messageCallback = callback;
}
