#include <Arduino.h>
#include "connections.h"

const char* wifiSSID = "Wu";
const char* wifiPassword = "Welcome98!";

const char* mqttServer = "192.168.68.60";
const char* mqttUserName= "";
const char* mqttPassword = "";


#ifdef TLS
WiFiClientSecure wifiClient;
#else
WiFiClient wifiClient;
#endif

PubSubClient mqttClient;

String macAddress;
char mac[50];
char clientId[50];


void setupConnections() {
  #ifdef TLS
    wifiClient.setCACert(rootCABuff);
    wifiClient.setInsecure();
  #endif
  mqttClient.setKeepAlive(20);
  mqttClient.setClient(wifiClient);
  Serial.begin(115200);

  wifiConnect();
  macAddress = WiFi.macAddress();
  macAddress.toCharArray(clientId, macAddress.length() + 1);

  Serial.print("MQTT server: ");
  Serial.println(mqttServer);
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(callback);

  strcpy(fop, platform);
  mqttReconnect();
}

void wifiConnect() {
  Serial.print("Connecting to WiFi ");
  Serial.print(wifiSSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPassword);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    disconnectLEDs();
  }
  Serial.println(" connected");
}

void mqttReconnect() {
  long r = random(1000);
  sprintf(clientId, "owlcms-%ld", r);
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnect();
  }
  while (!mqttClient.connected()) { 
    Serial.print(macAddress);
    Serial.print(" connecting to MQTT server...");

    if (mqttClient.connect(clientId, mqttUserName, mqttPassword)) {
      Serial.println(" connected");
      char requestTopic[50];
      sprintf(requestTopic, "owlcms/decisionRequest/%s/+", fop);
      mqttClient.subscribe(requestTopic);

      char ledTopic[50];
      sprintf(ledTopic, "owlcms/led/#", fop);
      mqttClient.subscribe(ledTopic);

      char summonTopic[50];
      sprintf(summonTopic, "owlcms/summon/#", fop);
      mqttClient.subscribe(summonTopic);

      char resetTopic[50];
      sprintf(resetTopic, "owlcms/reset/");
      mqttClient.subscribe(resetTopic);

    } else {
      Serial.print("MQTT connection failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 second");
      disconnectLEDs();
      //delay(5000);
    }
  }
}

void disconnectLEDs() {
  for(int dutyCycle = 0; dutyCycle <= 255; dutyCycle++){   
    analogWrite(ledPins[0], dutyCycle);
    delay(10);
  }

  for(int dutyCycle = 255; dutyCycle >= 0; dutyCycle--){
    analogWrite(ledPins[0], dutyCycle);
    delay(10);
  }
}