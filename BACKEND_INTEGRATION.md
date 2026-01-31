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
  "raw_kg": 150,
  "water_out_kg": 0
}
```

### 2. Outbound Process

#### a) Automatic fish detection
- System in IDLE waits for weight ≥ MIN_WEIGHT
- Saves `fish_kg` automatically

#### b) Ice and water dispensing
- Dispenses ice → saves `ice_out_kg`
- Fills water → saves `water_out_kg`

#### c) ID Validation
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

#### d) Data update
If validation is successful, sends output data:

```cpp
PUT /api/totes/TOTE001
{
  "fish_kg": 200,
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
    "fish_kg": 200,
    "raw_kg": 150,
    "ice_out_kg": 20,
    "water_out_kg": 40,
    "temp_out": 0.0,
    "created_at": "2026-01-08T10:30:00.000Z"
  }
}
```

## Configuration

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
  uint32_t fish_kg, 
  uint32_t ice_out_kg, 
  uint32_t water_out_kg, 
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
Payload: {"fish_kg":200,"ice_out_kg":20,"water_out_kg":40,"temp_out":0.0}
HTTP Response code: 200
Response: {"message":"Tote updated successfully","tote":{...}}
Backend updated successfully!
✓ Tote data sent to backend successfully!
```

### Connection error:
```
=== Updating Backend ===
PUT: http://192.168.100.10:3000/api/totes/TOTE001
Payload: {"fish_kg":200,"ice_out_kg":20,"water_out_kg":40,"temp_out":0.0}
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
      │                        │  {fish_kg,ice_out...} │
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
