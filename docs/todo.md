## NEW STRUCTURE ##

enum class ToteState {
  IDLE,
  DISPENSING_ICE,
  SETTLING_ICE,
  DISPENSING_WATER,
  SETTLING_WATER,
  WAITING_TOTE_ID,
  COMPLETED,
  PAUSED,
  CANCELED,
  ERROR
};

## handleToteState ##

- [x] Eliminar `WAITING_START`; `IDLE` espera START directamente
- [x] Agregar settling para hielo y agua
- [x] Agregar pausa/reanudar con START durante dispensing
- [x] Agregar una funcion de Waiting tote
- un Struct para los errores 
- Un logger para esto
