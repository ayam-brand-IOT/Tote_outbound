// ============================================================
// Settings.cpp  —  NVS-persisted runtime configuration
// ============================================================
#include "Settings.h"

namespace Settings {

  static Preferences _prefs;
  static volatile float _iceKg    = (float)TARGET_ICE_KG;
  static volatile float _waterKg  = (float)TARGET_WATER_KG;
  static volatile float _minWeight= (float)MIN_WEIGHT;

  void load() {
    _prefs.begin("tote_cfg", /*readOnly=*/true);
    _iceKg     = _prefs.getFloat("ice_kg",   (float)TARGET_ICE_KG);
    _waterKg   = _prefs.getFloat("water_kg", (float)TARGET_WATER_KG);
    _minWeight = _prefs.getFloat("min_w",    (float)MIN_WEIGHT);
    _prefs.end();
    Serial.printf("[Settings] Loaded  ice=%.2f kg  water=%.2f kg  min=%.2f kg\n",
                  _iceKg, _waterKg, _minWeight);
  }

  void save(float iceKg, float waterKg, float minWeight) {
    _iceKg     = iceKg;
    _waterKg   = waterKg;
    _minWeight = minWeight;
    _prefs.begin("tote_cfg", /*readOnly=*/false);
    _prefs.putFloat("ice_kg",   iceKg);
    _prefs.putFloat("water_kg", waterKg);
    _prefs.putFloat("min_w",    minWeight);
    _prefs.end();
    Serial.printf("[Settings] Saved   ice=%.2f kg  water=%.2f kg  min=%.2f kg\n",
                  iceKg, waterKg, minWeight);
  }

  float getTargetIceKg()  { return _iceKg;    }
  float getTargetWaterKg(){ return _waterKg;   }
  float getMinWeight()    { return _minWeight; }

} // namespace Settings
