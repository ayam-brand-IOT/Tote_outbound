## NEW STRUCTURE ##

enum class ToteState {
  IDLE,
  WAITING_START,
  DISPENSING_ICE,
  DISPENSING_WATER,
  WAITING_TOTE_ID,
  COMPLETED,
  CANCELED,
  ERROR
};

## handleToteState ##

- Agregar un limit para el waiting Start
- Agregar una funcion de Wainting tote
- un Struct para los errores 
- Un logger para esto