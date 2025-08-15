#ifndef CONNECTIONS_H
#define CONNECTIONS_H

#include <WiFi.h>
#include "PubSubClient.h"
#include "battery.h"
#include "interface.h"

#ifdef TLS
#include <WiFiClientSecure.h>
#include "certificates.h"
const int mqttPort = 8883;
#else
#include <WiFi.h>
const int mqttPort = 1883;
#endif

#ifdef TLS
extern WiFiClientSecure wifiClient;
#else
extern WiFiClient wifiClient;
#endif

extern PubSubClient mqttClient;
extern String macAddress;
extern char mac[50];
extern char clientId[50];
extern const char* platform;  
extern char fop[20];  
extern int referee;

// WiFi credentials (declared in connections.cpp)
extern const char* wifiSSID;
extern const char* wifiPassword;

void setupConnections();
void wifiConnect();
void mqttReconnect();
void callback(char* topic, byte* payload, unsigned int length);

#endif