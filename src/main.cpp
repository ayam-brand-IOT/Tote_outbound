#include "main.h"

Scheduler runner;
Controller controller;
TaskHandle_t detached_task;

// Function prototypes
void startICEPump();
void stopICEPump();

// Stages
Stage stage_1(2, initStage1, destroyStage1);
Stage stage_2(2, initStage2, destroyStage2);
Stage stage_3(2, initStage3, destroyStage3);

Task buttons_routine(20, TASK_FOREVER, &onButtonPressed);
Task ice_start_pulse(200, TASK_ONCE, []() {
  controller.writeDigitalOutput(ICE_PUMP, LOW);
  Serial.println("Ice pump start pulse ended");
});
Task ice_stop_pulse(200, TASK_ONCE, []() {
  controller.writeDigitalOutput(ICE_STOP, LOW);
  Serial.println("Ice pump stop pulse ended");
});
Task auto_stop_ice_routine(100, TASK_ONCE, []() {
  stopICEPump();
  Serial.println("Ice pump turned off");
});
Task stop_water_routine(100, TASK_ONCE, []() {
  controller.writeDigitalOutput(WATER_PUMP, LOW);
  Serial.println("Water pump turned off");
});

Task broadcast_weight_routine(200, TASK_FOREVER, []() {
  static float last_weight = 0;
  static uint32_t last_broadcast = 0;
  const uint32_t now = millis();

  const float current_weight = controller.getWeight();
  if (isnan(current_weight)) {
    Serial.println("Weight reading is NaN, skipping broadcast");
    return;
  }
    const float DELTA = 0.02f;  // 20 g de cambio mínimo
    bool weight_changed = isnan(last_weight) || fabs(current_weight - last_weight) >= DELTA;
    bool time_elapsed   = (now - last_broadcast) >= 1000;

  if (weight_changed || time_elapsed) {
    last_weight = current_weight;
    last_broadcast = now;
    controller.broadcastWeight(current_weight);
  }
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
  for (auto &b : buttons) b.button.begin();

  controller.init();
  controller.setUpWiFi(U_SSID, U_PASS, "tote-inbound");
  controller.connectToWiFi(/* web_server */ true, /* web_serial */ true, /* OTA */ true);
  controller.wifi.addToteIDcallback(&setToteID);

  xTaskCreatePinnedToCore(communicationTask, "communicationTask", 12000, NULL, 1, &detached_task, 0);

  runner.init();
  runner.addTask(buttons_routine);
  runner.addTask(ice_start_pulse);
  runner.addTask(ice_stop_pulse);
  runner.addTask(auto_stop_ice_routine);
  runner.addTask(stop_water_routine);
  runner.addTask(broadcast_weight_routine);
  buttons_routine.enable();
  broadcast_weight_routine.enable();

  delay(1000);
  Serial.println("Starting...");
}

void loop() {
  delay(20);
  const ControllerState current_state = controller.getState();

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
      // Esperar a que el peso sea mayor a MIN_WEIGHT
      onIDLE();
      break;
    
    case ToteState::DISPENSING_ICE:
      onIceFilling();
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
      // TODO: mostrar error, esperar intervención
      break;
  }
}


void communicationTask(void* pvParameters) {
  for (;;) {
    controller.WiFiLoop();
    
    if(controller.isWiFiConnected()) {
      controller.loopOTA();
    }
    // printStackUsage(); // Monitorea el uso de la pila
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
    
  //   // Guardar peso del pescado
  //   tote.fish_kg = current_weight;
    
  //   // Setear tara para siguiente medición
  //   controller.setTare();
    
  //   // Pasar a dispensar hielo
  //   toteState = ToteState::DISPENSING_ICE;
  //   Serial.println("Transitioning to DISPENSING_ICE");
  // }
}

void onWaterFilling() {
  // Init stage 2 (antes era stage 1)
  if (stage_2.getCurrentStep() == 0) {
    stage_2.init();
    stage_2.nextStep();
    waterTimer = millis();
  }

  else if (stage_2.getCurrentStep() == 1){
    if (!controller.hasIntervalPassed(waterTimer, 4000, false)) {
      Serial.print(".");
      return;
    }
    Serial.println();

    stage_2.nextStep();
  }

  if (stage_2.getCurrentStep() == 2) {
    stage_2.destroy();
    // Después del agua, esperar el ID del tote
    toteState = ToteState::WAITING_TOTE_ID;
    Serial.println("Transitioning to WAITING_TOTE_ID");
  }
}

void onIceFilling() {
  // Init stage 1 (ahora el hielo es primero)
  if (stage_1.getCurrentStep() == 0) {
    stage_1.init();
    stage_1.nextStep();
    iceTimer = millis();
  }

  else if (stage_1.getCurrentStep() == 1){
    if (!controller.hasIntervalPassed(iceTimer, 4000, false)) {
      Serial.print(".");
      return;
    }
    Serial.println();
    stage_1.nextStep();
  }

  if (stage_1.getCurrentStep() == 2) {
    stage_1.destroy();
    // Después del hielo, llenar agua
    toteState = ToteState::DISPENSING_WATER;
    Serial.println("Transitioning to DISPENSING_WATER");
  }
}

void onWaitingToteID() {
  static uint32_t lastPrompt = 0;
  
  // Mostrar prompt cada 2 segundos
  if (millis() - lastPrompt > 2000) {
    Serial.println("Waiting for Tote ID...");
    Serial.println("Fish: " + String(tote.fish_kg) + " kg");
    Serial.println("Ice: " + String(tote.ice_out_kg) + " kg");
    Serial.println("Water: " + String(tote.water_out_kg) + " kg");
    lastPrompt = millis();
  }
  
  // La transición a COMPLETED se hace desde setToteIdFromUI
}

void onToteReady() {
  // Init stage 3
  if (stage_3.getCurrentStep() == 0) {
    stage_3.init();
    stage_3.nextStep();
  }

  if (stage_3.getCurrentStep() == 1) {
    delay(1000);
    Serial.println("Tote completed and sent!");
    stage_3.nextStep();
  }

  if (stage_3.getCurrentStep() == 2) {
    stage_3.destroy();
    // Volver a IDLE para esperar el siguiente tote
    toteState = ToteState::IDLE;
    Serial.println("\n=== Ready for next tote ===");
  }
}

void onCanceled() {
  Serial.println("Tote canceled, cleaning up...");
  
  // Detener bombas
  stopICEPump();
  controller.writeDigitalOutput(WATER_PUMP, LOW);
  
  // Limpiar datos
  tote = {0, 0, 0, 0, 0};
  controller.setTare();
  
  // Resetear stages
  stage_1.destroy();
  stage_2.destroy();
  stage_3.destroy();
  
  // Volver a IDLE
  toteState = ToteState::IDLE;
  Serial.println("Returned to IDLE");
}

void onButtonPressed() {
  handleInputs();
  readButtonTypeFromSerial(); // Read button type from serial input
}

void initStage1() {
  Serial.println("\n=== Stage 1: Dispensing Ice ===");
  controller.setTare();
  startICEPump();
}

void initStage2() {
  Serial.println("\n=== Stage 2: Filling Water ===");
  controller.setTare();
  controller.writeDigitalOutput(WATER_PUMP, HIGH);
}

void initStage3() {
  Serial.println("Stage 3 Tote Ready");
 
}

void destroyStage1() {
  const uint32_t ice_out_kg = controller.getWeight();
  Serial.print("Ice dispensed: ");
  Serial.print(ice_out_kg);
  Serial.println(" kg");

  tote.ice_out_kg = ice_out_kg;

  stopICEPump();

  Serial.println("Ice dispensing completed");
  Serial.println("Stage 1 destroyed");
}

void destroyStage2() {
  const uint32_t water_out_kg = controller.getWeight();

  Serial.print("Water filled: ");
  Serial.print(water_out_kg);
  Serial.println(" kg");

  tote.water_out_kg = water_out_kg;

  controller.writeDigitalOutput(WATER_PUMP, LOW);

  Serial.println("Water filling completed");
  Serial.println("Stage 2 destroyed");
} 

void destroyStage3() {
  // Mostrar todos los datos del tote completado
  Serial.println("\n=== Tote Summary ===");
  Serial.print("ID: ");
  Serial.println(tote.id);
  Serial.print("Fish: ");
  Serial.print(tote.fish_kg);
  Serial.println(" kg");
  Serial.print("Ice: ");
  Serial.print(tote.ice_out_kg);
  Serial.println(" kg");
  Serial.print("Water: ");
  Serial.print(tote.water_out_kg);
  Serial.println(" kg");
  Serial.println("==================\n");
  
  // TODO: Aquí enviar datos al backend
  // sendToteDataToBackend(tote);
  
  // Reset the tare
  controller.setTare();
  
  // Limpiar datos para el siguiente tote
  tote = {0, 0, 0, 0, 0};
  Serial.println("Stage 3 destroyed");
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
    Serial.println("Already in process");
    return;
  }

  Serial.println("\n=== System Started ===");
  Serial.println("Waiting for tote with fish...");
  Serial.print("Minimum weight required: ");
  Serial.print(MIN_WEIGHT);
  Serial.println(" kg");
  
  const float current_weight = controller.getWeight();
  
  if (current_weight >= MIN_WEIGHT) {
    Serial.println("\n=== Tote detected! ===");
    Serial.print("Fish weight: ");
    Serial.print(current_weight);
    Serial.println(" kg");
    
    // Guardar peso del pescado
    tote.fish_kg = current_weight;
    
    // Setear tara para siguiente medición
    controller.setTare();
    
    // Pasar a dispensar hielo
    toteState = ToteState::DISPENSING_ICE;
    Serial.println("Transitioning to DISPENSING_ICE");
  }
}

void onStop() {
  Serial.println("\n=== STOP pressed ===");
  
  // Detener todas las bombas
  stopICEPump();
  controller.writeDigitalOutput(WATER_PUMP, LOW);
  auto_stop_ice_routine.cancel();
  stop_water_routine.cancel();
  
  // Cancelar el proceso actual
  toteState = ToteState::CANCELED;
}

void onManualIce() {
  Serial.println("Manual Ice");
  startICEPump();
  auto_stop_ice_routine.restartDelayed(5000);
}

void onManualWater() {
  Serial.println("Manual Water");
  controller.writeDigitalOutput(WATER_PUMP, HIGH);
  stop_water_routine.restartDelayed(5000);
  
}

void readButtonTypeFromSerial() {
  if (Serial.available()) {
    const uint8_t buttonType = Serial.parseInt();

    if (buttonType >= 0 && buttonType < BTN_COUNT) { // Valid button types are 0 to 5
      Serial.print("Button type received: ");
      Serial.println(buttonType);
      handleInputs(static_cast<button_type>(buttonType));
    }
    else {
      Serial.println("Invalid button type. Please enter a number between 0 and 5.");
    }
  }
}


void setToteID(const String& id) {
  if (id.length() >= ID_SIZE) {
    Serial.println("Tote ID is too long");
    return;
  }

  strncpy(tote.id, id.c_str(), ID_SIZE);
  tote.id[ID_SIZE - 1] = '\0'; // Ensure null termination
  Serial.print("Tote ID set to: ");
  Serial.println(tote.id);
}

bool setToteIdFromUI(const String& toteId) {
  if (toteState != ToteState::WAITING_TOTE_ID) {
    Serial.println("Cannot set ID, not in WAITING_TOTE_ID state");
    return false;
  }

  // Copiar ID al struct tote
  memset(tote.id, 0, sizeof(tote.id));
  toteId.substring(0, sizeof(tote.id)-1).toCharArray(tote.id, sizeof(tote.id));

  Serial.print("Tote ID received: ");
  Serial.println(tote.id);

  // Transición a COMPLETED
  toteState = ToteState::COMPLETED;
  return true;
}
