# Tote Outbound - Backend Integration Guide

## Backend Integration

This document describes how the **tote_outbound** system integrates with the tote management backend.

## Integration Flow

### 1. Prerequisite: Tote must exist in Backend
The tote must have been previously created by the **tote_inbound** system using:
```
POST /api/totes
{
  "tote_id": "TOTE001",
  "tote_kg": 100,
  "water_kg": 50,
  "ice_kg": 30,
  "raw_kg": 180
}
```

### 2. Outbound Process

#### a) Start and raw-weight capture
- System in IDLE waits for START and verifies weight ≥ MIN_WEIGHT
- Saves current `raw_kg` before outbound additions
- Applies tare so water/ice dispensing deltas start from the current load

#### b) Water and ice dispensing
- Fills water by weight target
- Waits `DISPENSE_SETTLING_MS`, then saves final `water_out_kg`
- Dispenses ice by weight with predictive STOP compensation
- Waits `DISPENSE_SETTLING_MS`, then saves final `ice_out_kg`
- Updates learned residual ice-tail compensation after each settled ice measurement

#### c) Pause / resume
- START during `DISPENSING_WATER` or `DISPENSING_ICE` pauses the active dispenser and enters `PAUSED`.
- START during `PAUSED` resumes the saved dispensing state.
- START is ignored for pause purposes during `SETTLING_WATER`, `SETTLING_ICE`, `WAITING_TOTE_ID`, `COMPLETED`, `CANCELED`, and `ERROR`.
- STOP cancels the cycle and performs cleanup.

#### d) ID Validation
When the operator enters the Tote ID, the system **validates against the backend**:

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

**Error response (404)**: ID rejected, system remains waiting

#### e) Data update
If validation is successful, sends output data:

```cpp
PUT /api/totes/TOTE001
{
  "raw_kg": 200,
  "ice_out_kg": 20,
  "water_out_kg": 40,
  "temp_out": 0.0
}
```

**Successful response (200)**:
```json
{
  "message": "Tote updated successfully",
  "tote": {
    "tote_id": "TOTE001",
    "tote_kg": 100,
    "water_kg": 50,
    "ice_kg": 30,
    "fish_kg": null,
    "raw_kg": 200,
    "ice_out_kg": 20,
    "water_out_kg": 40,
    "temp_out": 0.0,
    "created_at": "2026-01-08T10:30:00.000Z"
  }
}
```

## Configuration

### Dispensing behavior

Water and ice are controlled by weight, not by fixed dispensing time. Both final values are recorded after the pump/ice stop request and a settling delay:

```cpp
#define DISPENSE_SETTLING_MS 8000UL
```

Ice uses predictive compensation for residual auger discharge:

```text
stop_threshold = target_ice_kg - expected_ice_tail_kg
```

The learned `expected_ice_tail_kg` value is persisted in NVS as `ice_tail`.

### Backend URL

Edit in `include/config.h`:

```cpp
#define BACKEND_HOST "192.168.100.10"  // Change to backend IP
#define BACKEND_PORT 3000
#define BACKEND_URL "http://" BACKEND_HOST ":3000"
```

### Verify Backend

From ESP32 terminal or from network:

```bash
# Verify backend is running
curl http://192.168.100.10:3000/api/totes

# Verify a specific tote
curl http://192.168.100.10:3000/api/totes/TOTE001
```

## Integration Functions

### validateToteIDFromBackend()

```cpp
bool validateToteIDFromBackend(const String& toteId)
```

- **Purpose**: Verify that the tote exists in the backend
- **Returns**: `true` if tote exists, `false` if it doesn't exist or there's an error
- **Timeout**: 5 seconds
- **Log**: Prints request and response details

### updateToteInBackend()

```cpp
bool updateToteInBackend(
  const char* toteId, 
  float raw_kg, 
  float ice_out_kg, 
  float water_out_kg, 
  float temp_out
)
```

- **Purpose**: Update tote data with output values
- **Returns**: `true` if update successful, `false` in case of error
- **Timeout**: 10 seconds
- **Log**: Prints JSON payload and response

## Log Examples

### Successful validation:
```
Validating Tote ID 'TOTE001' with backend...
GET: http://192.168.100.10:3000/api/totes/TOTE001
HTTP Response code: 200
Tote found in backend
Backend confirmed tote ID: TOTE001
Tote ID validated successfully!
```

### Failed validation:
```
Validating Tote ID 'TOTE999' with backend...
GET: http://192.168.100.10:3000/api/totes/TOTE999
HTTP Response code: 404
Tote ID not found (404)
ERROR: Tote ID not found in backend!
Please check the ID and try again.
```

### Successful update:
```
=== Updating Backend ===
PUT: http://192.168.100.10:3000/api/totes/TOTE001
Payload: {"raw_kg":200,"ice_out_kg":20,"water_out_kg":40,"temp_out":0.0}
HTTP Response code: 200
Response: {"message":"Tote updated successfully","tote":{...}}
Backend updated successfully!
✓ Tote data sent to backend successfully!
```

### Connection error:
```
=== Updating Backend ===
PUT: http://192.168.100.10:3000/api/totes/TOTE001
Payload: {"raw_kg":200,"ice_out_kg":20,"water_out_kg":40,"temp_out":0.0}
HTTP PUT failed, error: connection refused
✗ Failed to send tote data to backend
  Data will be lost. Please check backend connection.
```

## Error Handling

### WiFi not connected
- Both functions check `controller.isWiFiConnected()`
- If no connection, return `false` immediately
- Don't attempt HTTP requests

### Timeout
- GET: 5 seconds
- PUT: 10 seconds
- After timeout, returns `false`

### Backend not available
- If backend doesn't respond or isn't running
- System shows error but **doesn't block** the operation
- Data is lost (TODO: implement offline queue)

### ID not found (404)
- System rejects the ID
- Remains in WAITING_TOTE_ID state
- Operator must enter a valid ID

## Troubleshooting

### "WiFi not connected, cannot validate ID"
**Problem**: ESP32 is not connected to WiFi
**Solution**: 
- Verify WiFi credentials in config.h
- Verify network is available
- Restart device

### "HTTP GET failed, error: connection refused"
**Problem**: Backend is not running or not accessible
**Solution**:
- Verify Docker is running: `docker-compose ps`
- Verify backend IP in config.h
- Ping backend: `ping 192.168.100.10`

### "Tote ID not found (404)"
**Problem**: Tote was not created by inbound system
**Solution**:
- Verify inbound created the tote first
- Query backend: `curl http://BACKEND_IP:3000/api/totes/TOTE_ID`
- Use a valid ID that exists in the system

### "Failed to send tote data to backend"
**Problem**: Error making PUT request
**Solution**:
- Verify connectivity with backend
- Check backend logs for error
- Data is lost - consider implementing offline queue

## Dependencies

### Required Libraries

- `HTTPClient.h` - HTTP Client (included in ESP32)
- `ArduinoJson.h` - JSON Serialization/Deserialization

Already included in `platformio.ini`:
```ini
lib_deps = 
    bblanchon/ArduinoJson@6.20.0
```

## Complete Inbound → Backend → Outbound Integration

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
      │                        │  {raw_kg,ice_out...}  │
      │                        │<──────────────────────│
      │                        │                       │
      │                        │  200 OK {updated}     │
      │                        │──────────────────────>│
      │                        │                       │
```

## TODO / Future Improvements

- [ ] Implement offline queue for later synchronization
- [ ] Add automatic retries on failed requests
- [ ] Implement real temperature reading (temp_out)
- [ ] Cache list of valid IDs for faster validation
- [ ] Add communication statistics with backend
- [ ] Implement heartbeat with backend
