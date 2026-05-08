#include "IceCompensation.h"
#include "Settings.h"
#include "Debug.h"

#include <Arduino.h>

namespace {
  float stopRequestIceKg = NAN;

  float clampTail(float tailKg) {
    if (isnan(tailKg)) return (float)ICE_TAIL_KG_DEFAULT;
    return constrain(tailKg, (float)ICE_TAIL_KG_MIN, (float)ICE_TAIL_KG_MAX);
  }
}

namespace IceCompensation {

  void resetCycle() {
    stopRequestIceKg = NAN;
  }

  float getStopThresholdKg() {
    const float targetKg = Settings::getTargetIceKg();
    const float thresholdKg = targetKg - clampTail(Settings::getExpectedIceTailKg());
    return max(0.0f, thresholdKg);
  }

  bool shouldRequestStop(float currentIceKg) {
    if (isnan(currentIceKg)) return false;
    return currentIceKg >= getStopThresholdKg();
  }

  void recordStopRequest(float currentIceKg) {
    stopRequestIceKg = currentIceKg;
    LOG_MAIN("Ice predictive stop requested at %.2f kg (target %.2f kg, expected tail %.2f kg)\n",
             currentIceKg,
             Settings::getTargetIceKg(),
             Settings::getExpectedIceTailKg());
  }

  void learnFromSettledWeight(float settledIceKg) {
    if (isnan(stopRequestIceKg) || isnan(settledIceKg)) {
      LOG_MAIN("Ice tail learning skipped: missing stop or settled weight\n");
      resetCycle();
      return;
    }

    const float measuredTailKg = settledIceKg - stopRequestIceKg;
    if (measuredTailKg < ICE_TAIL_KG_MIN || measuredTailKg > ICE_TAIL_KG_MAX) {
      LOG_MAIN("Ice tail learning skipped: measured tail %.2f kg outside %.2f-%.2f kg\n",
               measuredTailKg,
               (float)ICE_TAIL_KG_MIN,
               (float)ICE_TAIL_KG_MAX);
      resetCycle();
      return;
    }

    const float previousTailKg = clampTail(Settings::getExpectedIceTailKg());
    const float learnedTailKg = clampTail(
      previousTailKg + ((float)ICE_TAIL_LEARN_ALPHA * (measuredTailKg - previousTailKg))
    );

    Settings::saveExpectedIceTailKg(learnedTailKg);
    LOG_MAIN("Ice tail learned: measured %.2f kg, expected %.2f -> %.2f kg\n",
             measuredTailKg,
             previousTailKg,
             learnedTailKg);
    resetCycle();
  }
}
