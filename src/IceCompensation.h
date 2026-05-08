#pragma once

namespace IceCompensation {
  void resetCycle();
  float getStopThresholdKg();
  bool shouldRequestStop(float currentIceKg);
  void recordStopRequest(float currentIceKg);
  void learnFromSettledWeight(float settledIceKg);
}
