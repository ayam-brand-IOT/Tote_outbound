#include "Controller.h"

// Configuración Modbus RTU para RS-485
// Pines: RX=IO18 (U1RXD), TX=IO17 (U1TXD), DE/RE=IO8 (RS485_RTS)
// Slave ID = 1 (según el simulador)
MarelClient marel(1, 18, 17, 8);


void Controller::init(){
  // resetStrapedpins();
  Serial.begin(115200);
  // setUpIOS();
  // setUpI2C();
  setUpDevice();
  // setUpRTC();
}

void Controller::resetStrapedpins() {
  // Asegura que los pines strap de la EdgeBox estén en estado conocido (INPUT con pull-down)
    gpio_reset_pin(GPIO_NUM_40);  // DO_0
    gpio_reset_pin(GPIO_NUM_39);  // DO_1
    gpio_reset_pin(GPIO_NUM_38);  // DO_2
}

void Controller::setupPinMode( uint8_t pin, gpio_mode_t mode){
  gpio_config_t io_conf = {};
  io_conf.intr_type    = GPIO_INTR_DISABLE;
  io_conf.mode         = mode;
  io_conf.pin_bit_mask = (1ULL << pin);
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.pull_up_en   = GPIO_PULLUP_DISABLE;
  gpio_config(&io_conf);
}

void Controller::task() {
  marel.task();  // Procesar comunicación Modbus
}

void Controller::setUpIOS(){
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
  float weight = marel.getNetWeightKg(); // p.ej. "0.00" o "76.4" (peso neto = bruto - tara)
  DEBUG_M(("Raw Weight: " + String(weight)).c_str());

  if (isnan(weight)) {
    DEBUG_M("Failed to get weight from Marel");
    return NAN;
  }

  // Muestra con 2 decimales sí o sí
  Serial.printf("Parsed Weight: %.2f\n", weight); // o Serial.println(val, 2);
  return weight;

}


void Controller::setUpDigitalOutputs() {
    for (auto &output : outputs) {
        setupPinMode(output, GPIO_MODE_OUTPUT);
        gpio_set_level((gpio_num_t)output, LOW); // Asegura que inicien en LOW
    }
}

void Controller::setUpRTC(){
    
}

bool Controller::isRTCConnected(){
  return true;
}

bool Controller::readDigitalInput(uint8_t input){
  return gpio_get_level((gpio_num_t)input);
}

void Controller::writeDigitalOutput(uint8_t output, uint8_t value){
  gpio_set_level((gpio_num_t)output, value);
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