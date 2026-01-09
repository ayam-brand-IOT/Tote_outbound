# Tote Outbound - Backend Integration Guide

## Integración con Backend

Este documento describe cómo el sistema **tote_outbound** se integra con el backend de gestión de totes.

## Flujo de Integración

### 1. Prerequisito: Tote debe existir en Backend
El tote debe haber sido creado previamente por el sistema **tote_inbound** usando:
```
POST /api/totes
{
  "tote_id": "TOTE001",
  "tote_kg": 100,
  "water_kg": 50,
  "ice_kg": 30,
  "raw_kg": 150,
  "water_out_kg": 0
}
```

### 2. Proceso en Outbound

#### a) Detección automática de pescado
- Sistema en IDLE espera peso ≥ MIN_WEIGHT
- Guarda `fish_kg` automáticamente

#### b) Dispensado de hielo y agua
- Dispensa hielo → guarda `ice_out_kg`
- Llena agua → guarda `water_out_kg`

#### c) Validación de ID
Cuando el operador ingresa el Tote ID, el sistema **valida contra el backend**:

```cpp
GET /api/totes/TOTE001
```

**Respuesta exitosa (200)**:
```json
{
  "tote": {
    "tote_id": "TOTE001",
    "tote_kg": 100,
    "water_kg": 50,
    "ice_kg": 30,
    "fish_kg": null,
    "raw_kg": 150,
    "ice_out_kg": null,
    "water_out_kg": 0,
    "temp_out": null,
    "created_at": "2026-01-08T10:30:00.000Z"
  }
}
```

**Respuesta error (404)**: ID rechazado, sistema permanece esperando

#### d) Actualización de datos
Si la validación es exitosa, envía los datos de salida:

```cpp
PUT /api/totes/TOTE001
{
  "fish_kg": 200,
  "ice_out_kg": 20,
  "water_out_kg": 40,
  "temp_out": 0.0
}
```

**Respuesta exitosa (200)**:
```json
{
  "message": "Tote updated successfully",
  "tote": {
    "tote_id": "TOTE001",
    "tote_kg": 100,
    "water_kg": 50,
    "ice_kg": 30,
    "fish_kg": 200,
    "raw_kg": 150,
    "ice_out_kg": 20,
    "water_out_kg": 40,
    "temp_out": 0.0,
    "created_at": "2026-01-08T10:30:00.000Z"
  }
}
```

## Configuración

### Backend URL

Editar en `include/config.h`:

```cpp
#define BACKEND_HOST "192.168.100.10"  // Cambiar a IP del backend
#define BACKEND_PORT 3000
#define BACKEND_URL "http://" BACKEND_HOST ":3000"
```

### Verificar Backend

Desde la terminal del ESP32 o desde la red:

```bash
# Verificar que el backend esté corriendo
curl http://192.168.100.10:3000/api/totes

# Verificar un tote específico
curl http://192.168.100.10:3000/api/totes/TOTE001
```

## Funciones de Integración

### validateToteIDFromBackend()

```cpp
bool validateToteIDFromBackend(const String& toteId)
```

- **Propósito**: Verificar que el tote exista en el backend
- **Retorna**: `true` si el tote existe, `false` si no existe o hay error
- **Timeout**: 5 segundos
- **Log**: Imprime detalles de la request y response

### updateToteInBackend()

```cpp
bool updateToteInBackend(
  const char* toteId, 
  uint32_t fish_kg, 
  uint32_t ice_out_kg, 
  uint32_t water_out_kg, 
  float temp_out
)
```

- **Propósito**: Actualizar los datos del tote con valores de salida
- **Retorna**: `true` si actualización exitosa, `false` en caso de error
- **Timeout**: 10 segundos
- **Log**: Imprime payload JSON y response

## Ejemplo de Logs

### Validación exitosa:
```
Validating Tote ID 'TOTE001' with backend...
GET: http://192.168.100.10:3000/api/totes/TOTE001
HTTP Response code: 200
Tote found in backend
Backend confirmed tote ID: TOTE001
Tote ID validated successfully!
```

### Validación fallida:
```
Validating Tote ID 'TOTE999' with backend...
GET: http://192.168.100.10:3000/api/totes/TOTE999
HTTP Response code: 404
Tote ID not found (404)
ERROR: Tote ID not found in backend!
Please check the ID and try again.
```

### Actualización exitosa:
```
=== Updating Backend ===
PUT: http://192.168.100.10:3000/api/totes/TOTE001
Payload: {"fish_kg":200,"ice_out_kg":20,"water_out_kg":40,"temp_out":0.0}
HTTP Response code: 200
Response: {"message":"Tote updated successfully","tote":{...}}
Backend updated successfully!
✓ Tote data sent to backend successfully!
```

### Error de conexión:
```
=== Updating Backend ===
PUT: http://192.168.100.10:3000/api/totes/TOTE001
Payload: {"fish_kg":200,"ice_out_kg":20,"water_out_kg":40,"temp_out":0.0}
HTTP PUT failed, error: connection refused
✗ Failed to send tote data to backend
  Data will be lost. Please check backend connection.
```

## Manejo de Errores

### WiFi no conectado
- Ambas funciones verifican `controller.isWiFiConnected()`
- Si no hay conexión, retornan `false` inmediatamente
- No intentan hacer requests HTTP

### Timeout
- GET: 5 segundos
- PUT: 10 segundos
- Después del timeout, retorna `false`

### Backend no disponible
- Si el backend no responde o no está corriendo
- El sistema muestra error pero **no bloquea** la operación
- Los datos se pierden (TODO: implementar cola offline)

### ID no encontrado (404)
- El sistema rechaza el ID
- Permanece en estado WAITING_TOTE_ID
- El operador debe ingresar un ID válido

## Troubleshooting

### "WiFi not connected, cannot validate ID"
**Problema**: ESP32 no está conectado a WiFi
**Solución**: 
- Verificar credenciales WiFi en config.h
- Verificar que la red esté disponible
- Reiniciar el dispositivo

### "HTTP GET failed, error: connection refused"
**Problema**: Backend no está corriendo o no es accesible
**Solución**:
- Verificar que Docker esté corriendo: `docker-compose ps`
- Verificar IP del backend en config.h
- Hacer ping al backend: `ping 192.168.100.10`

### "Tote ID not found (404)"
**Problema**: El tote no fue creado por el sistema inbound
**Solución**:
- Verificar que el inbound haya creado el tote primero
- Consultar el backend: `curl http://BACKEND_IP:3000/api/totes/TOTE_ID`
- Usar un ID válido que exista en el sistema

### "Failed to send tote data to backend"
**Problema**: Error al hacer PUT request
**Solución**:
- Verificar conectividad con el backend
- Verificar logs del backend para ver el error
- Los datos se pierden - considerar implementar cola offline

## Dependencias

### Librerías requeridas

- `HTTPClient.h` - Cliente HTTP (incluido en ESP32)
- `ArduinoJson.h` - Serialización/Deserialización JSON

Ya están incluidas en `platformio.ini`:
```ini
lib_deps = 
    bblanchon/ArduinoJson@6.20.0
```

## Integración Completa Inbound → Backend → Outbound

```
┌─────────────┐         ┌─────────────┐         ┌──────────────┐
│   INBOUND   │         │   BACKEND   │         │   OUTBOUND   │
│             │         │   (MySQL)   │         │              │
└─────┬───────┘         └──────┬──────┘         └──────┬───────┘
      │                        │                       │
      │ POST /api/totes        │                       │
      │ {tote_id, tote_kg,...} │                       │
      │───────────────────────>│                       │
      │                        │                       │
      │      201 Created       │                       │
      │<───────────────────────│                       │
      │                        │                       │
      │                        │  GET /api/totes/:id   │
      │                        │<──────────────────────│
      │                        │                       │
      │                        │  200 OK {tote}        │
      │                        │──────────────────────>│
      │                        │                       │
      │                        │  PUT /api/totes/:id   │
      │                        │  {fish_kg,ice_out...} │
      │                        │<──────────────────────│
      │                        │                       │
      │                        │  200 OK {updated}     │
      │                        │──────────────────────>│
      │                        │                       │
```

## TODO / Mejoras Futuras

- [ ] Implementar cola offline para sincronización posterior
- [ ] Agregar reintentos automáticos en requests fallidos
- [ ] Implementar lectura de temperatura real (temp_out)
- [ ] Cachear lista de IDs válidos para validación más rápida
- [ ] Agregar estadísticas de comunicación con backend
- [ ] Implementar heartbeat con backend
