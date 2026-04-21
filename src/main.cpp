#include "main.h"
#include "Settings.h"
#include "Debug.h"

Scheduler runner;
Controller controller;
TaskHandle_t detached_task;
ToteWebSocketClient wsClient;  // WebSocket client instance
BLEQRClient bleQRClient;       // BLE Central – connects to QR-Reader-OUT peripheral

// Function prototypes
void startICEPump();
void stopICEPump();
void onSettlingIce();

// Stages
Stage stage_1(2, initStage1, destroyStage1);
Stage stage_2(2, initStage2, destroyStage2);
Stage stage_3(2, initStage3, destroyStage3);

Task buttons_routine(20, TASK_FOREVER, &onButtonPressed);
Task ice_start_pulse(200, TASK_ONCE, []() {
  controller.writeDigitalOutput(ICE_PUMP, LOW);
  LOG_CTRL("Ice pump start pulse ended\n");
});
Task ice_stop_pulse(200, TASK_ONCE, []() {
  controller.writeDigitalOutput(ICE_STOP, LOW);
  LOG_CTRL("Ice pump stop pulse ended\n");
});
Task auto_stop_ice_routine(100, TASK_ONCE, []() {
  stopICEPump();
  LOG_CTRL("Ice pump turned off\n");
});
Task stop_water_routine(100, TASK_ONCE, []() {
  controller.writeDigitalOutput(WATER_PUMP, LOW);
  LOG_CTRL("Water pump turned off\n");
});

Task broadcast_weight_routine(200, TASK_FOREVER, []() {
  static float last_weight = 0;
  static uint32_t last_broadcast = 0;
  const uint32_t now = millis();

  const float current_weight = controller.getWeight();
  if (isnan(current_weight)) {
    LOG_MAIN("Weight reading is NaN, skipping broadcast\n");
    return;
  }
    const float DELTA = 0.02f;  // 20 g minimum change
    bool weight_changed = isnan(last_weight) || fabs(current_weight - last_weight) >= DELTA;
    bool time_elapsed   = (now - last_broadcast) >= 1000;

  if (weight_changed || time_elapsed) {
    last_weight = current_weight;
    last_broadcast = now;
    // Send weight via WebSocket instead of HTTP broadcast
    wsClient.sendWeight(current_weight);
  }
});

// ── Indicator tasks ────────────────────────────────────────────────────────────
// Ticks cada 250 ms; las distintas tasas de parpadeo se obtienen con módulo.
//   INDICATOR_1 = estado del sistema   INDICATOR_2 = estado BLE QR reader
Task indicator_task(250, TASK_FOREVER, []() {
  static uint8_t tick = 0;
  tick++;

  // ── INDICATOR_1: Estado del sistema ───────────────────────────────────
  uint8_t ind1 = LOW;
  switch (toteState) {
    case ToteState::IDLE:
      ind1 = LOW;                              // Apagado
      break;
    case ToteState::DISPENSING_ICE:
    case ToteState::DISPENSING_WATER:
      ind1 = (tick % 2) ? HIGH : LOW;          // Parpadeo rápido 500 ms
      break;
    case ToteState::SETTLING_ICE:
      ind1 = (tick % 4 < 2) ? HIGH : LOW;     // Parpadeo medio 1 s (bomba apagada, asentando)
      break;
    case ToteState::WAITING_TOTE_ID:
      ind1 = (tick % 8 < 4) ? HIGH : LOW;     // Parpadeo lento 1 s
      break;
    case ToteState::COMPLETED:
      ind1 = HIGH;                             // Encendido fijo
      break;
    case ToteState::CANCELED:
    case ToteState::ERROR: {                   // Triple flash + 2 s OFF
      uint8_t p = tick % 20;                  // Ciclo 5 s
      ind1 = (p < 6 && p % 2 == 0) ? HIGH : LOW;
      break;
    }
  }
  controller.writeDigitalOutput(INDICATOR_1, ind1);

  // ── INDICATOR_2: Estado BLE QR reader ─────────────────────────────
  uint8_t ind2 = LOW;
  switch (bleQRClient.getState()) {
    case BLEQRState::CONNECTED:
      ind2 = HIGH;                             // Encendido fijo: lector listo
      break;
    case BLEQRState::SCANNING:
    case BLEQRState::CONNECTING:
    case BLEQRState::LOST:
      ind2 = (tick % 8 < 4) ? HIGH : LOW;     // Parpadeo lento: buscando
      break;
    default:                                   // IDLE: apagado
      ind2 = LOW;
      break;
  }
  controller.writeDigitalOutput(INDICATOR_2, ind2);
});

button_action stop_btn          = {STOP, STOP_IO, onStop};
button_action start_btn         = {START, START_IO, onStart};
button_action manual_ice_btn    = {MANUAL_ICE, MANUAL_ICE_IO, onManualIce};
button_action manual_water_btn  = {MANUAL_WATER, MANUAL_WATER_IO, onManualWater};

Stage stages[] = {stage_1, stage_2, stage_3};
button_action buttons[] = { stop_btn, start_btn, manual_ice_btn, manual_water_btn };

uint32_t iceTimer = 0UL;
uint32_t waterTimer = 0UL;
tote_data tote = {0, 0, 0, 0, 0};

void startICEPump() {
  controller.writeDigitalOutput(ICE_PUMP, HIGH);
  ice_start_pulse.restartDelayed(200);
}

void stopICEPump() {
  controller.writeDigitalOutput(ICE_STOP, HIGH);
  ice_stop_pulse.restartDelayed(200);
}

void setup() {
  controller.init();
  Settings::load();  // Load persisted ice/water/min-weight targets from NVS

  for (auto &b : buttons) b.button.begin();
  controller.setUpWiFi(U_SSID, U_PASS, "tote-outbound");
  controller.connectToWiFi(/* web_server */ true, /* web_serial */ true, /* OTA */ true);
  
  // Initialize WebSocket client
  wsClient.begin(BACKEND_HOST, BACKEND_WS_PORT, "/esp32");
  wsClient.setMessageCallback(onWebSocketMessage);

  // Initialize BLE QR client – scans for "QR-Reader-OUT" peripheral
  bleQRClient.begin([](const String& qr) -> bool {
    if (qr == "NO_QR") {
      LOG_BLE("[BLE-QR-OUT] No QR in reader buffer yet\n");
      return false;
    }
    LOG_BLE("[BLE-QR-OUT] QR received via BLE: %s\n", qr.c_str());
    return setToteIdFromUI(qr);  // true = procesado → BLEQRClient enviará ACK
  });

  controller.setUpIOS();

  xTaskCreatePinnedToCore(communicationTask, "communicationTask", 12000, NULL, 1, &detached_task, 0);

  runner.init();
  runner.addTask(buttons_routine);
  runner.addTask(ice_start_pulse);
  runner.addTask(ice_stop_pulse);
  runner.addTask(auto_stop_ice_routine);
  runner.addTask(stop_water_routine);
  runner.addTask(broadcast_weight_routine);
  runner.addTask(indicator_task);
  buttons_routine.enable();
  broadcast_weight_routine.enable();
  indicator_task.enable();

  controller.setupPinMode(AO_0, GPIO_MODE_OUTPUT);
  gpio_set_level((gpio_num_t)AO_0, HIGH);

  controller.writeDigitalOutput(ICE_STOP, HIGH);
  delay(500);
  controller.writeDigitalOutput(ICE_STOP, LOW);

  LOG_MAIN("Starting...\n");

}

void loop() {
  delay(20);
  const ControllerState current_state = controller.getState();

  controller.task();   // Process Modbus communication
  wsClient.loop();     // Process WebSocket communication
  bleQRClient.loop();  // Drive BLE scan / connect state machine
  runner.execute();

  handleToteState();


  // switch (current_state) {
  //   case IDLE:
  //     // FUCK off

  //     break;  
  //   case WATER_FILLING:
  //     onWaterFilling();
  //     break;
  //   case ICE_FILLING:
  //     onIceFilling();
  //     break;
  //   case TOTE_READY:
  //     onToteReady();
  //     break;
  //   default:
  //     break;
  // }
}

void handleToteState(){
    switch (toteState) {
    case ToteState::IDLE:
      // Wait for weight to be greater than MIN_WEIGHT
      onIDLE();
      break;
    
    case ToteState::DISPENSING_ICE:
      onIceFilling();
      break;

    case ToteState::SETTLING_ICE:
      onSettlingIce();
      break;

    case ToteState::DISPENSING_WATER:
      onWaterFilling();
      break;

    case ToteState::WAITING_TOTE_ID:
      onWaitingToteID();
      break;

    case ToteState::COMPLETED:
      onToteReady();
      break;

    case ToteState::CANCELED:
      onCanceled();
      break;

    case ToteState::ERROR:
      // TODO: show error, wait for intervention
      break;
  }
}


void communicationTask(void* pvParameters) {
  for (;;) {
    controller.WiFiLoop();
    
    if(controller.isWiFiConnected()) {
      controller.loopOTA();
    }
    // printStackUsage(); // Monitor stack usage
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void onIDLE() {
  // static uint32_t lastCheck = 0;
  
  // // Verificar peso cada 500ms
  // if (millis() - lastCheck < 500) {
  //   return;
  // }
  // lastCheck = millis();
  
  // const float current_weight = controller.getWeight();
  
  // if (current_weight >= MIN_WEIGHT) {
  //   Serial.println("\n=== Tote detected! ===");
  //   Serial.print("Fish weight: ");
  //   Serial.print(current_weight);
  //   Serial.println(" kg");
    
  //   // Save fish weight
  //   tote.fish_kg = current_weight;
    
  //   // Set tare for next measurement
  //   controller.setTare();
    
  //   // Proceed to ice dispensing
  //   toteState = ToteState::DISPENSING_ICE;
  //   Serial.println("Transitioning to DISPENSING_ICE");
  // }
}

void onWaterFilling() {
  // Stage 1: water is now first
  if (stage_1.getCurrentStep() == 0) {
    stage_1.init();
    stage_1.nextStep();
    wsClient.sendStateChange("DISPENSING_WATER");
  }

  else if (stage_1.getCurrentStep() == 1) {
    const float current_weight = controller.getWeight();
    const float weight_delta = current_weight - tote.initial_weight;

    if (weight_delta < Settings::getTargetWaterKg()) {
      static uint32_t lastPrint = 0;
      if (millis() - lastPrint > 500) {
        LOG_MAIN("Water: %.2f / %.2f kg\r", weight_delta, Settings::getTargetWaterKg());
        lastPrint = millis();
      }
      return;
    }
    LOG_MAIN("✓ Target water weight reached: %.2f kg\n", weight_delta);
    stage_1.nextStep();
  }

  if (stage_1.getCurrentStep() == 2) {
    stage_1.destroy();
    // After water, dispense ice
    toteState = ToteState::DISPENSING_ICE;
    wsClient.sendStateChange("DISPENSING_ICE");
    LOG_MAIN("Transitioning to DISPENSING_ICE\n");
  }
}

void onIceFilling() {
  // Stage 2: ice is now second (after water)
  if (stage_2.getCurrentStep() == 0) {
    stage_2.init();
    stage_2.nextStep();
    wsClient.sendStateChange("DISPENSING_ICE");
  }

  else if (stage_2.getCurrentStep() == 1) {
    const float current_weight = controller.getWeight();
    // weight_delta is cumulative (water + ice); subtract water to get ice only
    const float weight_delta = current_weight - tote.initial_weight;
    const float ice_delta = weight_delta - Settings::getTargetWaterKg();
    const float target_total = Settings::getTargetWaterKg() + Settings::getTargetIceKg();

    if (weight_delta < target_total) {
      static uint32_t lastPrint = 0;
      if (millis() - lastPrint > 500) {
        LOG_MAIN("Ice: %.2f / %.2f kg\r", ice_delta, Settings::getTargetIceKg());
        lastPrint = millis();
      }
      return;
    }
    LOG_MAIN("✓ Target ice weight reached: %.2f kg\n", ice_delta);
    stage_2.nextStep();
  }

  if (stage_2.getCurrentStep() == 2) {
    stage_2.destroy();
    // Settling: let residual ice finish falling
    toteState = ToteState::SETTLING_ICE;
    LOG_MAIN("Transitioning to SETTLING_ICE (8 s debounce)\n");
  }
}

void onSettlingIce() {
  static uint32_t settleStart = 0;
  if (settleStart == 0) {
    settleStart = millis();
    wsClient.sendStateChange("SETTLING_ICE");
    LOG_MAIN("Settling: waiting 8 s for residual ice to stop falling...\n");
  }
  if (millis() - settleStart >= 8000UL) {
    settleStart = 0;  // reset for next cycle
    toteState = ToteState::WAITING_TOTE_ID;
    wsClient.sendStateChange("WAITING_TOTE_ID");
    LOG_MAIN("Transitioning to WAITING_TOTE_ID\n");
  }
}

void onWaitingToteID() {
  static uint32_t lastPrompt = 0;

  // Show prompt every 3 seconds
  if (millis() - lastPrompt > 3000) {
    const bool bleReady = bleQRClient.isConnected();
    LOG_MAIN("\n╔════════════════════════════════════╗\n");
    LOG_MAIN("║   WAITING FOR TOTE ID              ║\n");
    LOG_MAIN("╠════════════════════════════════════╣\n");
    LOG_MAIN("║ Raw:   %.2f kg\n", tote.raw_kg);
    LOG_MAIN("║ Ice:   %.2f kg\n", tote.ice_out_kg);
    LOG_MAIN("║ Water: %.2f kg\n", tote.water_out_kg);
    LOG_MAIN("╠════════════════════════════════════╣\n");
    if (bleReady) {
      LOG_MAIN("║ [BLE]  QR-Reader-OUT conectado ✓   ║\n");
      LOG_MAIN("║        Leyendo QR automáticamente  ║\n");
    } else {
      LOG_MAIN("║ [BLE]  QR-Reader-OUT no conectado  ║\n");
    }
    LOG_MAIN("║ [WEB]  Captura con cámara del tel  ║\n");
    LOG_MAIN("╚════════════════════════════════════╝\n\n");

    // If BLE reader is connected, request the buffered QR every 3 s
    if (bleReady) {
      bleQRClient.requestQR();
    }

    lastPrompt = millis();
  }

  // Transition to COMPLETED is handled by setToteIdFromUI() (BLE or web path)
}

void onToteReady() {
  // Init stage 3
  if (stage_3.getCurrentStep() == 0) {
    stage_3.init();
    stage_3.nextStep();
    wsClient.sendToteCompleted(tote.id);
  }

  if (stage_3.getCurrentStep() == 1) {
    delay(1000);
    LOG_MAIN("Tote completed and sent!\n");
    stage_3.nextStep();
  }

  if (stage_3.getCurrentStep() == 2) {
    stage_3.destroy();
    // Return to IDLE to wait for next tote
    toteState = ToteState::IDLE;
    wsClient.sendStateChange("IDLE");
    LOG_MAIN("\n=== Ready for next tote ===\n");
  }
}

void onCanceled() {
  LOG_MAIN("Tote canceled, cleaning up...\n");
  
  // Stop pumps
  stopICEPump();
  controller.writeDigitalOutput(WATER_PUMP, LOW);
  
  // Clear data
  tote = {0, 0, 0, 0, 0};
  controller.setTare();
  
  // Reset stages
  stage_1.destroy();
  stage_2.destroy();
  stage_3.destroy();
  
  // Return to IDLE
  toteState = ToteState::IDLE;
  LOG_MAIN("Returned to IDLE\n");
}

void onButtonPressed() {
  handleInputs();
  readButtonTypeFromSerial(); // Read button type from serial input
}

void initStage1() {
  LOG_MAIN("\n=== Stage 1: Filling Water ===\n");
  // Save initial weight to calculate delta (workaround if TARE doesn't work)
  tote.initial_weight = controller.getWeight();
  LOG_MAIN("Initial weight saved: %.2f kg\n", tote.initial_weight);
  controller.setTare();  // Try TARE anyway
  controller.writeDigitalOutput(WATER_PUMP, HIGH);
}

void initStage2() {
  LOG_MAIN("\n=== Stage 2: Dispensing Ice ===\n");
  // Do NOT setTare here - weight is cumulative (water already dispensed)
  startICEPump();
}

void initStage3() {
  LOG_MAIN("Stage 3 Tote Ready\n");
 
}

void destroyStage1() {
  // After TARE at start, getWeight() - initial_weight = water dispensed
  const float water_out_kg = controller.getWeight() - tote.initial_weight;
  LOG_MAIN("Water filled: %.2f kg\n", water_out_kg);

  tote.water_out_kg = water_out_kg;

  controller.writeDigitalOutput(WATER_PUMP, LOW);

  wsClient.sendWaterDispensed(tote.water_out_kg);

  LOG_MAIN("Water filling completed\n");
  LOG_MAIN("Stage 1 destroyed\n");
}

void destroyStage2() {
  // Cumulative delta minus water already dispensed = ice only
  const float ice_out_kg = controller.getWeight() - tote.initial_weight - tote.water_out_kg;
  LOG_MAIN("Ice dispensed: %.2f kg\n", ice_out_kg);

  tote.ice_out_kg = ice_out_kg;

  stopICEPump();

  wsClient.sendIceDispensed(tote.ice_out_kg);

  LOG_MAIN("Ice dispensing completed\n");
  LOG_MAIN("Stage 2 destroyed\n");
} 

void destroyStage3() {
  // Show all completed tote data
  LOG_MAIN("\n=== Tote Summary ===\n");
  LOG_MAIN("ID:    %s\n",   tote.id);
  LOG_MAIN("Raw:   %.2f kg\n", tote.raw_kg);
  LOG_MAIN("Water: %.2f kg\n", tote.water_out_kg);
  LOG_MAIN("Ice:   %.2f kg\n", tote.ice_out_kg);
  LOG_MAIN("==================\n\n");
  
  float temp_out = 0.0;
  
  bool success = updateToteInBackend(
    tote.id,
    tote.raw_kg,
    tote.ice_out_kg,
    tote.water_out_kg,
    temp_out
  );
  
  if (success) {
    LOG_MAIN("✓ Tote data sent to backend successfully!\n");
  } else {
    LOG_ERR("✗ Failed to send tote data to backend\n");
    LOG_ERR("  Data will be lost. Please check backend connection.\n");
  }
  
  // Reset the tare
  controller.clearTare();
  
  // Clear data for next tote
  tote = {0, 0, 0, 0, 0};
  LOG_MAIN("Stage 3 destroyed\n");
}

button_type handleInputs(button_type override){
  // iterate buttons
  for (auto &btn : buttons) {
    if (btn.button.released() || override == btn.type) {
      btn.handler();
      return btn.type;
    }
  }
  return NONE;
}

void onStart() {
  if (toteState != ToteState::IDLE) {
    LOG_MAIN("Already in process\n");
    return;
  }

  LOG_MAIN("\n=== System Started ===\n");
  LOG_MAIN("Minimum weight required: %.2f kg\n", Settings::getMinWeight());
  
  const float current_weight = controller.getWeight();
  
  if (current_weight >= Settings::getMinWeight()) {
    // Store total weight as-is — backend calculates fish_kg
    tote.raw_kg = current_weight;
    LOG_MAIN("Raw weight (fish + tote + inbound residues): %.2f kg\n", tote.raw_kg);
    
    // Set tare so dispensing deltas start from zero
    controller.setTare();
    
    toteState = ToteState::DISPENSING_WATER;
    LOG_MAIN("Transitioning to DISPENSING_WATER\n");
  } else {
    LOG_MAIN("Weight too low (%.2f kg), waiting for tote\n", current_weight);
  }
}

void onStop() {
  LOG_MAIN("\n=== STOP pressed ===\n");
  
  // Stop all pumps
  stopICEPump();
  controller.writeDigitalOutput(WATER_PUMP, LOW);
  auto_stop_ice_routine.cancel();
  stop_water_routine.cancel();
  
  // Cancel current process
  toteState = ToteState::CANCELED;
}

void onManualIce() {
  LOG_MAIN("Manual Ice\n");
  startICEPump();
  auto_stop_ice_routine.restartDelayed(5000);
}

void onManualWater() {
  LOG_MAIN("Manual Water\n");
  controller.writeDigitalOutput(WATER_PUMP, HIGH);
  stop_water_routine.restartDelayed(5000);
  
}

void readButtonTypeFromSerial() {
  if (Serial.available()) {
    const uint8_t buttonType = Serial.parseInt();

    if (buttonType >= 0 && buttonType < BTN_COUNT) { // Valid button types are 0 to 5
      LOG_MAIN("Button type received: %d\n", buttonType);
      handleInputs(static_cast<button_type>(buttonType));
    }
    else {
      LOG_ERR("Invalid button type. Please enter a number between 0 and 5.\n");
    }
  }
}


void setToteID(const String& id) {
  if (id.length() >= ID_SIZE) {
    LOG_ERR("Tote ID is too long\n");
    return;
  }

  strncpy(tote.id, id.c_str(), ID_SIZE);
  tote.id[ID_SIZE - 1] = '\0'; // Ensure null termination
  LOG_MAIN("Tote ID set to: %s\n", tote.id);
}

bool setToteIdFromUI(const String& toteId) {
  if (toteState != ToteState::WAITING_TOTE_ID) {
    LOG_MAIN("Cannot set ID, not in WAITING_TOTE_ID state\n");
    return false;
  }

  // Validate that the ID exists in backend
  LOG_MAIN("Validating Tote ID '%s' with backend...\n", toteId.c_str());
  
  if (!validateToteIDFromBackend(toteId)) {
    LOG_ERR("ERROR: Tote ID not found in backend!\n");
    LOG_ERR("Please check the ID and try again.\n");
    wsClient.sendError("Tote ID not found in backend");
    return false;
  }
  
  LOG_MAIN("Tote ID validated successfully!\n");

  // Copy ID to tote struct
  memset(tote.id, 0, sizeof(tote.id));
  toteId.substring(0, sizeof(tote.id)-1).toCharArray(tote.id, sizeof(tote.id));

  LOG_MAIN("Tote ID set to: %s\n", tote.id);

  // Send validation via WebSocket
  wsClient.sendToteValidated(tote.id);

  // Transition to COMPLETED
  toteState = ToteState::COMPLETED;
  wsClient.sendStateChange("COMPLETED");
  return true;
}

// ==================== Backend API Functions ====================

bool validateToteIDFromBackend(const String& toteId) {
  if (!controller.isWiFiConnected()) {
    LOG_ERR("WiFi not connected, cannot validate ID\n");
    return false;
  }

  HTTPClient http;
  String url = String(BACKEND_URL) + "/api/totes/" + toteId;
  
  LOG_MAIN("GET: %s\n", url.c_str());
  
  http.begin(url);
  http.setTimeout(5000); // 5 seconds timeout
  
  int httpCode = http.GET();
  
  if (httpCode > 0) {
    LOG_MAIN("HTTP Response code: %d\n", httpCode);
    
    if (httpCode == 200) {
      String payload = http.getString();
      LOG_MAIN("Tote found in backend\n");
      
      // Parse JSON response
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, payload);
      
      if (!error) {
        const char* id = doc["tote"]["tote_id"];
        LOG_MAIN("Backend confirmed tote ID: %s\n", id);
        
        // Extract raw_kg from backend to calculate fish weight later
        if (doc["tote"].containsKey("raw_kg")) {
          tote.raw_kg = doc["tote"]["raw_kg"].as<float>();
          LOG_MAIN("Raw weight from inbound: %.2f kg\n", tote.raw_kg);
        }
      }
      
      http.end();
      return true;
    }
    else if (httpCode == 404) {
      LOG_ERR("Tote ID not found (404)\n");
      http.end();
      return false;
    }
  }
  else {
    LOG_ERR("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  
  http.end();
  return false;
}

bool updateToteInBackend(const char* toteId, float raw_kg, float ice_out_kg, float water_out_kg, float temp_out) {
  if (!controller.isWiFiConnected()) {
    LOG_ERR("WiFi not connected, cannot update backend\n");
    return false;
  }

  HTTPClient http;
  String url = String(BACKEND_URL) + "/api/totes/" + String(toteId);
  
  LOG_MAIN("\n=== Updating Backend ===\n");
  LOG_MAIN("PUT: %s\n", url.c_str());
  
  // Create JSON payload
  DynamicJsonDocument doc(256);
  doc["raw_kg"]       = raw_kg;
  doc["ice_out_kg"]   = ice_out_kg;
  doc["water_out_kg"] = water_out_kg;
  doc["temp_out"]     = temp_out;
  
  String jsonPayload;
  serializeJson(doc, jsonPayload);
  
  LOG_MAIN("Payload: %s\n", jsonPayload.c_str());
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(10000); // 10 seconds timeout
  
  int httpCode = http.PUT(jsonPayload);
  
  if (httpCode > 0) {
    LOG_MAIN("HTTP Response code: %d\n", httpCode);
    
    String response = http.getString();
    LOG_MAIN("Response: %s\n", response.c_str());
    
    if (httpCode == 200) {
      LOG_MAIN("Backend updated successfully!\n");
      http.end();
      return true;
    }
  }
  else {
    LOG_ERR("HTTP PUT failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  
  http.end();
  return false;
}

// WebSocket message handler
void onWebSocketMessage(String type, JsonDocument& doc) {
  LOG_WS("WebSocket message received: %s\n", type.c_str());
  
  if (type == "qr_scanned") {
    const char* toteId = doc["toteId"];
    if (toteId && strlen(toteId) > 0) {
      LOG_MAIN("QR scanned from browser: %s\n", toteId);
      
      // Use setToteIdFromUI to properly handle state transition
      if (setToteIdFromUI(String(toteId))) {
        LOG_MAIN("Tote ID set successfully via WebSocket\n");
      } else {
        LOG_ERR("Failed to set tote ID - wrong state or validation failed\n");
        setToteID(String(toteId));
      }
    }
  }
  else if (type == "command") {
    const char* command = doc["command"];
    LOG_MAIN("Command received: %s\n", command);
    
    // Handle commands from backend/browser
    if (strcmp(command, "start") == 0) {
      if (toteState == ToteState::IDLE) {
        toteState = ToteState::DISPENSING_WATER;
        wsClient.sendStateChange("DISPENSING_WATER");
      }
    }
    else if (strcmp(command, "stop") == 0) {
      toteState = ToteState::CANCELED;
      wsClient.sendStateChange("CANCELED");
    }
  }
  else if (type == "update_settings") {
    const float ice   = doc["ice_kg"]   | Settings::getTargetIceKg();
    const float water = doc["water_kg"] | Settings::getTargetWaterKg();
    const float minW  = doc["min_w"]    | Settings::getMinWeight();
    Settings::save(ice, water, minW);
    // Echo back the saved values so the browser panel can confirm
    wsClient.sendSettingsCurrent(ice, water, minW);
  }
  else if (type == "get_settings") {
    wsClient.sendSettingsCurrent(
      Settings::getTargetIceKg(),
      Settings::getTargetWaterKg(),
      Settings::getMinWeight()
    );
  }
}
