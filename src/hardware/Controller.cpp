#include "Controller.h"

byte mac[] = MAC_ADDRESS;
IPAddress ip(CLIENT_IP);
IPAddress gateway(CLIENT_GATEWAY);
IPAddress subnet(SUBNET_ADDRESS);

MarelClient marel(SERVER_IP, SERVER_PORT, mac, ip, gateway, subnet);


void Controller::init(){
  setUpIOS();
  // setUpI2C();
  setUpDevice();
  // setUpRTC();
}

void Controller::setUpIOS(){
  Serial.begin(115200);

  setUpDigitalOutputs();
}

void Controller::setUpI2C(){

}

void Controller::loopOTA() {
  wifi.loopOTA();
}

void Controller::WiFiLoop() {
  wifi.loopWS();
  if (!isWiFiConnected()) {
    reconnectWiFi();
    vTaskDelay(500 / portTICK_PERIOD_MS);
    return;
  }
}

void Controller::reconnectWiFi() {
  wifi.reconnect();
}

bool Controller::isWiFiConnected() {
  return wifi.isConnected();
}

void Controller::setUpWiFi(const char* ssid, const char* password, const char* hostname) {
  wifi.init(ssid, password, hostname);
}

void Controller::connectToWiFi(bool web_server, bool web_serial, bool OTA) {
  wifi.connectToWiFi();
  if(OTA) wifi.setUpOTA();
  if(web_server) wifi.setUpWebServer(web_serial);
}

void Controller::setState(ControllerState state){
  this->state = state;
  toteState = ToteState::DISPENSING_ICE;
  
  DEBUG_M(("State changed from " + String((int)this->state) + " to " + String((int)state)).c_str());
}

ControllerState Controller::getState(){
  return this->state;
}

void Controller::setUpDevice(){
  marel.begin();
}

bool Controller::setTare(){
  marel.setTare();
  return true;
}

float Controller::getWeight(){
  float weight = marel.getWeightKg(); // p.ej. "0.00" o "76.4"
  DEBUG_M(("Raw Weight: " + String(weight)).c_str());

  if (isnan(weight)) {
    DEBUG_M("Failed to get weight from Marel");
    return NAN;
  }

  // Muestra con 2 decimales sí o sí
  Serial.printf("Parsed Weight: %.2f\n", weight); // o Serial.println(val, 2);
  return weight;

}



void Controller::setUpDigitalOutputs(){
  for (auto &output : outputs) pinMode(output, OUTPUT);
}

void Controller::setUpRTC(){
    
}

bool Controller::isRTCConnected(){
  return true;
}

bool Controller::readDigitalInput(uint8_t input){
  return digitalRead(input);
}

void Controller::writeDigitalOutput(uint8_t output, uint8_t value){
  return digitalWrite(output, value);
}

void Controller::broadcastWeight(float weight){
  wifi.broadcastWeight(weight);
}

bool Controller::hasIntervalPassed(uint32_t &previousMillis, uint32_t interval, bool to_min) {
    if(to_min) interval *= 60000UL;
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;
      return true;
    }
    return false; 
}