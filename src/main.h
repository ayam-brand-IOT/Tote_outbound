#include "Stage.h"
#include "Types.h"
#include <TaskScheduler.h>
#include "hardware/Controller.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

void onStop();
void onStart();
void onIDLE();
void onCanceled();
void onManualIce();
void onManualWater();
void onButtonPressed();
void onWaitingToteID();
void setToteID(const String& id);
void handleToteState();
bool setToteIdFromUI(const String& toteId);
button_type handleInputs(button_type override = NONE);

void handleIDLE();
void initStage1();
void initStage2();
void initStage3();

void destroyStage1();
void destroyStage2();
void destroyStage3();
void onToteReady();
void onIceFilling();
void onWaterFilling();
void readButtonTypeFromSerial();
void communicationTask(void* pvParameters);

// Backend API functions
bool validateToteIDFromBackend(const String& toteId);
bool updateToteInBackend(const char* toteId, uint32_t fish_kg, uint32_t ice_out_kg, uint32_t water_out_kg, float temp_out);