#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H

#include <WebSocketsClient.h>
#include <ArduinoJson.h>

class ToteWebSocketClient {
private:
    WebSocketsClient webSocket;
    bool isConnected;
    unsigned long lastHeartbeat;
    unsigned long reconnectDelay;
    unsigned long lastReconnectAttempt;
    int reconnectAttempts;
    
    static const unsigned long HEARTBEAT_INTERVAL = 30000; // 30 seconds
    static const unsigned long MAX_RECONNECT_DELAY = 30000; // 30 seconds
    static const int MAX_RECONNECT_ATTEMPTS = 10;
    
    // Callback function pointer
    void (*messageCallback)(String type, JsonDocument& doc);
    
    // Static wrapper for WebSocket event handler
    static void webSocketEventStatic(WStype_t type, uint8_t * payload, size_t length);
    static ToteWebSocketClient* instance;
    
    void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);
    void handleMessage(String payload);
    void sendHeartbeat();
    
public:
    ToteWebSocketClient();
    
    bool begin(const char* host, uint16_t port, const char* path = "/");
    void loop();
    bool sendWeight(float weight);
    bool sendStateChange(const char* state);
    bool sendToteValidated(const char* toteId);
    bool sendToteCompleted(const char* toteId);
    bool sendIceDispensed(float ice_kg);
    bool sendWaterDispensed(float water_kg);
    bool sendError(const char* message);
    
    bool isClientConnected() { return isConnected; }
    void setMessageCallback(void (*callback)(String type, JsonDocument& doc));
};

#endif // WEBSOCKET_CLIENT_H
