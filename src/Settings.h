#pragma once
// ============================================================
// Settings  —  NVS-persisted runtime configuration
// Namespace: "tote_cfg"   Keys: ice_kg | water_kg | min_w
// ============================================================
#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

namespace Settings {
  /**
   * Load values from NVS.  Falls back to compile-time defaults
   * defined in config.h when no saved value exists.
   * Call once in setup(), before any logic that reads targets.
   */
  void  load();

  /**
   * Persist new values to NVS and update the in-memory cache.
   * Takes effect immediately — no reboot required.
   */
  void  save(float iceKg, float waterKg, float minWeight);

  float getTargetIceKg();   ///< Target ice dispensing weight (kg)
  float getTargetWaterKg(); ///< Target water filling weight (kg)
  float getMinWeight();     ///< Minimum tote weight to begin cycle (kg)
}
