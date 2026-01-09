#pragma once

#include <Arduino.h>
#include "EdgeBox_ESP_100.h"

#define MIN_WEIGHT 5

// ###################### INPUTS ######################
#define START_IO                DI_0
#define STOP_IO                 DI_1

#define MANUAL_ICE_IO           DI_3
#define MANUAL_WATER_IO         DI_2

// ###################### OUTPUTS ######################
#define ICE_PUMP                DO_0
#define ICE_STOP                DO_1
#define WATER_PUMP              DO_2


// #################### MAREL - INFO ####################
// Datos del servidor Marel
#define SERVER_PORT             5200
#define SERVER_IP               "192.168.1.7"
#define CLIENT_IP               {192, 168, 1, 29}
#define CLIENT_GATEWAY          {192, 168, 1, 254}       
#define MAC_ADDRESS             { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }


//Configuración de red
#define HAS_STATIC_IP                               //TURN ON THE STATIC IP
#define IP_ADDRESS { 192, 168, 100, 29 }
#define GATEWAY_ADDRESS { 192, 168, 1, 254 }
#define SUBNET_ADDRESS { 255, 255, 255, 0 }

//TURN ON THE STATIC IP


#define U_SSID "MFP-Guest24"
#define U_PASS "testing123"

// #define U_SSID "CFPP (Test)"
// #define U_PASS "cfpptest"

// ##################### BACKEND API #####################
#define BACKEND_HOST "192.168.100.10"  // Cambiar a la IP del backend
#define BACKEND_PORT 3000
#define BACKEND_URL "http://" BACKEND_HOST ":3000"

// ##################### WEB SERVER #####################

#define SSID_SIZE 32
#define ID_SIZE 32
#define PASSWORD_SIZE 64
#define HOSTNAME_SIZE 32
#define IP_ADDRESS_SIZE 16

#define www_username "admin"
#define www_password "admin"
#define VERSION "1.0.0"
#define SERVER_INDEX_HTML INDEX_HTML

