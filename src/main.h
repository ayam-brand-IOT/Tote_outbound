#include "Stage.h"
#include "Types.h"
#include <TaskScheduler.h>
#include "hardware/Controller.h"

void onStop();
void onStart();
void onIDLE();
void onManualIce();
void onManualWater();
void onButtonPressed();
void onWaitingToteID();
void onCanceled();
void setToteID(const String& id);
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