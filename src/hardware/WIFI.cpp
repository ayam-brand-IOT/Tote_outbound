// #include "WiFiType.h"
#include "WIFI.h"

AsyncWebServer server(80);

static void handle_update_progress_cb(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index){
    int cmd = (filename.indexOf("spiffs") > -1) ? U_SPIFFS : U_FLASH;
    // Update.runAsync(true);
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  }

  if (Update.write(data, len) != len) {
    Update.printError(Serial);
  }

  if (final) {
    if (!Update.end(true)){
      Update.printError(Serial);
    }
  }
}

  String WIFI::setLayOutInfo(const char* site, String extra_prop, String value){ 
    String html = site;

    if (extra_prop != "" && value != "") html.replace(extra_prop, value);
    

    html.replace("{{LOCATION}}", String(hostname));
    html.replace("{{VERSION}}", String(VERSION));

    return html;
  };


/* Message callback of WebSerial */
static void recvMsg(uint8_t *data, size_t len){
  // WebSerial.println("Received Data...");
  String d = "";
  for(int i=0; i < len; i++){
    d += char(data[i]);
  }
  // WebSerial.println(d);
}


void WIFI::init(const char* ssid, const char* password, const char* hostname, const char* static_ip) {
  strncpy(this->ssid, ssid, sizeof(this->ssid) - 1);
  this->ssid[sizeof(this->ssid) - 1] = '\0';  // Asegurarse de que esté terminado con '\0'

  strncpy(this->password, password, sizeof(this->password) - 1);
  this->password[sizeof(this->password) - 1] = '\0';  // Asegurarse de que esté terminado con '\0'

  strncpy(this->hostname, hostname, sizeof(this->hostname) - 1);
  this->hostname[sizeof(this->hostname) - 1] = '\0';  // Asegurarse de que esté terminado con '\0'

  // strncpy(this->static_ip, static_ip, sizeof(this->static_ip) - 1);
  // this->static_ip[sizeof(this->static_ip) - 1] = '\0';  // Asegurarse de que esté terminado con '\0'
}

void WIFI::setUpWebServer(bool brigeSerial){
  /*use mdns for host name resolution*/
  while (!MDNS.begin(hostname)){ // http://esp32.local
    DEBUG("Error setting up MDNS responder!");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  
  DEBUG("mDNS responder started Pinche Hugo");

  // Función para autenticación básica en todas las rutas
  auto checkAuth = [&](AsyncWebServerRequest *request) {
    if (!request->authenticate(www_username, www_password)) {
      request->requestAuthentication();
      return false;
    }
    return true;
  };


  // ======================== Static Files ========================

  // server.on("/style.css", HTTP_GET, [&checkAuth](AsyncWebServerRequest *request){
  //   if(!checkAuth(request)) return;
  //   request->send(200, "text/css", STYLE_CSS);
  // });



// ======================== Routes ========================

    /*return index page which is stored in serverIndex */
  server.on("/", HTTP_GET, [&](AsyncWebServerRequest *request) {
    if(!checkAuth(request)) return;
    const String doc = setLayOutInfo(SERVER_INDEX_HTML);
    request->send(200, "text/html", doc);
  });

  server.on("/register_pallet", HTTP_POST, [&](AsyncWebServerRequest *request) {
    if(!checkAuth(request)) return;

    if (!request->hasParam("id", true)) {
      request->send(400, "text/plain", "Missing id");
      return;
    }

    // String palletId = request->arg("id");
    String id = request->getParam("id", true)->value();


    bool ok = toteIDCallback(id);
    if (!ok) {
      // No estábamos en WAITING_TOTE_ID
      request->send(409, "text/plain", "Tote not ready to receive ID.");
      return;
    }

    request->send(200, "text/plain", "Tote registered successfully.");
  });


  server.onNotFound([](AsyncWebServerRequest *request) {
      // request->send(SPIFFS, request->url(), String(), false); <------ Buen pishi hack!
      request->send(404, "text/html", "Not found: <u>'"+ request->url() + "'</u>");
  });

  // ======================== SERVER PROCESES ========================
  
  server.on("/reset", HTTP_POST, [&checkAuth](AsyncWebServerRequest *request) {
    if(!checkAuth(request)) return;
    request->send(200, "text/plain", "Resetting...");
    ESP.restart();
  });

  ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
      Serial.println("WebSocket client connected");
    } else if (type == WS_EVT_DISCONNECT) {
      Serial.println("WebSocket client disconnected");
    }
  });

  server.addHandler(&ws);
  server.begin();
}

String WIFI::getIP(){
  String ip =  MDNS.queryHost("beer-control").toString();
  Serial.println(ip);
  return ip;
}

void WIFI::connectToWiFi(){
  // Set static IP if provided
  
  #ifdef HAS_STATIC_IP
  WiFi.config(IP_ADDRESS, GATEWAY_ADDRESS, SUBNET_ADDRESS, IPAddress(8, 8, 8, 8));
  #endif

  WiFi.begin(ssid, password);
  uint32_t notConnectedCounter = 0;
  EEPROM.begin(32);
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    DEBUG("Wifi connecting...");
      
    notConnectedCounter++;
    if(notConnectedCounter > 7) { // Reset board if not connected after 5s
      DEBUG("Resetting due to Wifi not connecting...");
      const uint8_t num_of_tries = EEPROM.readInt(1);
      if (num_of_tries == 3) break;          
      else {
        EEPROM.writeInt(1, num_of_tries + 1);
        EEPROM.commit();
        EEPROM.end();
        ESP.restart();          
      }
    }
  }

  EEPROM.writeInt(1, 0);
  EEPROM.commit();
  EEPROM.end();

  DEBUG(("IP address: " + WiFi.localIP().toString()).c_str());
}

void WIFI::setUpOTA(){
  if(isConnected()){
    ArduinoOTA.setHostname(hostname);
    ArduinoOTA.onStart([]() {
    String type;
    type = ArduinoOTA.getCommand() == U_FLASH ? "sketch" : "filesystem";
      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    logger.println("Start updating " + type);
    }).onEnd([]() {
      logger.println("\nEnd");
    }).onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    }).onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) logger.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) logger.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) logger.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) logger.println("Receive Failed");
      else if (error == OTA_END_ERROR) logger.println("End Failed");
    });
    ArduinoOTA.begin();
  }
}

void WIFI::loopOTA(){
  ArduinoOTA.handle();
}

bool WIFI::refreshWiFiStatus(){
  const bool connection = isConnected();
  if (connection != last_connection_state){
    last_connection_state = connection;
    return true;
  }
  return false;
}

bool WIFI::getConnectionStatus(){
  return last_connection_state;
}

bool WIFI::isConnected(){
  return WiFi.status() == WL_CONNECTED;
}

void WIFI::reconnect(){
  WiFi.begin(ssid, password);
  uint8_t timeout = 0;
  vTaskDelay(2000 / portTICK_PERIOD_MS);
  while ( WiFi.status() != WL_CONNECTED ){
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    log_i(" waiting on wifi connection" );
    timeout++;
    if (timeout == 2) return;
  }
}

void WIFI::DEBUG(const char *message){
  char buffer[100];
  snprintf(buffer, sizeof(buffer), "[WIFI]: %s", message);
  logger.println(buffer);
}

void WIFI::ERROR(ErrorType error){
  char buffer[100];
  snprintf(buffer, sizeof(buffer), " -> WIFI]: %s", errorMessages[error].c_str());
  logger.println(buffer);
}

void WIFI::loopWS(){
  ws.cleanupClients();
}

void WIFI::broadcastWeight(float weight){
  ws.textAll(String(weight));
}



