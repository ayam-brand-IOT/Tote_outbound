#ifndef MY_WIFI_H
#define MY_WIFI_H
#include "EEPROM.h"
#include <Update.h>
#include <SPIFFS.h>
#include "config.h"
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <WiFiMulti.h>
#include <WiFiClient.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include "resources/WebFiles.h"

// ERROR MESSAGES
#define ERR_WRONG_CREDENTIALS "Wrong credentials"
#define ERR_LOST_CONNECTION "Lost connection"

#define logger Serial

#ifdef WebSerial
  // No incluir WebSerialLite.h
#else
  
#endif
#include "ESPAsyncWebServer.h"


class WIFI {
  public:
    void init(const char* ssid, const char* password, const char* hostname, const char* static_ip = NULL);
    void loopOTA();
    String getIP();
    void setUpOTA();
    void reconnect();
    bool isConnected();
    
    void connectToWiFi();
    bool refreshWiFiStatus();
    bool getConnectionStatus();
    void setUpWebServer(bool brigeSerial = false);
    void loopWS();
    void broadcastWeight(float weight);

    void addToteIDcallback(bool (*callback)(const String&)) {
      if (callback == NULL) {
        DEBUG("Tote ID callback is NULL");
        return;
      }
      this->toteIDCallback = callback;
    }


    private:
    enum ErrorType { 
      WRONG_CREDENTIALS, 
      LOST_CONNECTION,
      NUM_ERRORS 
    };
    
    const String errorMessages[NUM_ERRORS] = {ERR_WRONG_CREDENTIALS, ERR_LOST_CONNECTION};

    char ssid[SSID_SIZE];  
    char password[PASSWORD_SIZE];
    char hostname[HOSTNAME_SIZE];  
    char static_ip[IP_ADDRESS_SIZE];

    bool (*toteIDCallback)(const String&) = NULL;
    bool last_connection_state = false;
    void DEBUG(const char *message);
    void ERROR(ErrorType error);
    String setLayOutInfo(const char* site, String extra_prop = "", String value = "");
    AsyncWebSocket ws = AsyncWebSocket("/ws");

};
#endif
