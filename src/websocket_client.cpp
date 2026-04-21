#include "websocket_client.h"
#include "Debug.h"

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
    LOG_WS("Connecting to WebSocket: ws://%s:%d%s\n", host, port, path);
    
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
            LOG_WS("[WS] Disconnected\n");
            isConnected = false;
            break;
            
        case WStype_CONNECTED: {
            LOG_WS("[WS] Connected to url: %s\n", payload);
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
            LOG_WS("[WS] Received: %s\n", payload);
            handleMessage(String((char*)payload));
            break;
            
        case WStype_ERROR:
            LOG_WS("[WS] Error \n");
            break;
            
        case WStype_PING:
            LOG_WS("[WS] Ping received \n");
            break;
            
        case WStype_PONG:
            LOG_WS("[WS] Pong received \n");
            break;
    }
}

void ToteWebSocketClient::handleMessage(String payload) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        LOG_WS("[WS] JSON parse error: %s\n", error.c_str());
        return;
    }
    
    const char* type = doc["type"];
    if (!type) {
        LOG_WS("[WS] Message missing 'type' field\n");
        return;
    }
    
    LOG_WS("[WS] Message type: %s\n", type);
    
    // Handle heartbeat response
    if (strcmp(type, "heartbeat") == 0) {
        LOG_WS("[WS] Heartbeat acknowledged\n");
        return;
    }
    
    // Handle command messages
    if (strcmp(type, "command") == 0) {
        const char* command = doc["command"];
        LOG_WS("[WS] Command received: %s\n", command);
        
        // Call user callback if set
        if (messageCallback) {
            messageCallback(String(type), doc);
        }
        return;
    }
    
    // Handle qr_scanned from browser
    if (strcmp(type, "qr_scanned") == 0) {
        const char* toteId = doc["toteId"];
        LOG_WS("[WS] QR scanned from browser: %s\n", toteId);
        
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
        LOG_WS("[WS] Not connected, cannot send weight\n");
        return false;
    }
    
    StaticJsonDocument<128> doc;
    doc["type"] = "weight_update";
    doc["station"] = "outbound";
    doc["weight"] = weight;
    
    String output;
    serializeJson(doc, output);
    
    webSocket.sendTXT(output);
    LOG_WS("[WS] Sent weight: %.2f kg\n", weight);
    
    return true;
}

bool ToteWebSocketClient::sendStateChange(const char* state) {
    if (!isConnected) {
        LOG_WS("[WS] Not connected, cannot send state\n");
        return false;
    }
    
    StaticJsonDocument<128> doc;
    doc["type"] = "state_change";
    doc["station"] = "outbound";
    doc["state"] = state;
    
    String output;
    serializeJson(doc, output);
    
    webSocket.sendTXT(output);
    LOG_WS("[WS] Sent state: %s\n", state);
    
    return true;
}

bool ToteWebSocketClient::sendToteValidated(const char* toteId) {
    if (!isConnected) {
        LOG_WS("[WS] Not connected, cannot send validation\n");
        return false;
    }
    
    StaticJsonDocument<128> doc;
    doc["type"] = "tote_validated";
    doc["station"] = "outbound";
    doc["toteId"] = toteId;
    
    String output;
    serializeJson(doc, output);
    
    webSocket.sendTXT(output);
    LOG_WS("[WS] Tote validated: %s\n", toteId);
    
    return true;
}

bool ToteWebSocketClient::sendToteCompleted(const char* toteId) {
    if (!isConnected) {
        LOG_WS("[WS] Not connected, cannot send completion\n");
        return false;
    }
    
    StaticJsonDocument<128> doc;
    doc["type"] = "tote_completed";
    doc["station"] = "outbound";
    doc["toteId"] = toteId;
    
    String output;
    serializeJson(doc, output);
    
    webSocket.sendTXT(output);
    LOG_WS("[WS] Tote completed: %s\n", toteId);
    
    return true;
}

bool ToteWebSocketClient::sendIceDispensed(float ice_kg) {
    if (!isConnected) {
        LOG_WS("[WS] Not connected, cannot send ice dispensed\n");
        return false;
    }
    
    StaticJsonDocument<128> doc;
    doc["type"] = "ice_dispensed";
    doc["station"] = "outbound";
    doc["ice_kg"] = ice_kg;
    
    String output;
    serializeJson(doc, output);
    
    webSocket.sendTXT(output);
    LOG_WS("[WS] Ice dispensed: %.2f kg\n", ice_kg);
    
    return true;
}

bool ToteWebSocketClient::sendWaterDispensed(float water_kg) {
    if (!isConnected) {
        LOG_WS("[WS] Not connected, cannot send water dispensed\n");
        return false;
    }
    
    StaticJsonDocument<128> doc;
    doc["type"] = "water_dispensed";
    doc["station"] = "outbound";
    doc["water_kg"] = water_kg;
    
    String output;
    serializeJson(doc, output);
    
    webSocket.sendTXT(output);
    LOG_WS("[WS] Water dispensed: %.2f kg\n", water_kg);
    
    return true;
}

bool ToteWebSocketClient::sendError(const char* message) {
    if (!isConnected) {
        LOG_WS("[WS] Not connected, cannot send error\n");
        return false;
    }
    
    StaticJsonDocument<256> doc;
    doc["type"] = "error";
    doc["station"] = "outbound";
    doc["message"] = message;
    
    String output;
    serializeJson(doc, output);
    
    webSocket.sendTXT(output);
    LOG_WS("[WS] Error sent: %s\n", message);
    
    return true;
}

bool ToteWebSocketClient::sendSettingsCurrent(float ice_kg, float water_kg, float min_w) {
    if (!isConnected) return false;
    
    StaticJsonDocument<192> doc;
    doc["type"]     = "settings_current";
    doc["station"]  = "outbound";
    doc["ice_kg"]   = ice_kg;
    doc["water_kg"] = water_kg;
    doc["min_w"]    = min_w;
    
    String output;
    serializeJson(doc, output);
    webSocket.sendTXT(output);
    LOG_WS("[WS] Settings broadcast: ice=%.2f water=%.2f min=%.2f\n", ice_kg, water_kg, min_w);
    return true;
}

void ToteWebSocketClient::setMessageCallback(void (*callback)(String type, JsonDocument& doc)) {
    messageCallback = callback;
}
