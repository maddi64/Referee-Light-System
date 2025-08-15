#include "connections.h"

const char* wifiSSID = "Wu";
const char* wifiPassword = "Welcome98!";

// const char* wifiSSID = "RLSX2-NSWWA001";
// const char* wifiPassword = "RepLogicNSWWA!";
// const char* mqttServer = "192.168.68.1";
const char* mqttServer = "192.168.68.50";
const char* mqttUserName= "";
const char* mqttPassword = "";

//int retryCounter = 0;

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
    delay(1000);
    Serial.print(".");
    // retryCounter++;
    // if (retryCounter == 15) {
    //   ESP.restart();
    // }
    updateDisplay(false, false, referee, getBatteryPercentage(), "", "WiFi connection lost");
  }
  Serial.println(" connected");
  updateDisplay(true, false, referee, getBatteryPercentage(), "", "");
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
    updateDisplay(true, false, referee, getBatteryPercentage(), "", "Server connection lost");

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
      sprintf(resetTopic, "owlcms/fop/resetDecisions/#", fop);
      mqttClient.subscribe(resetTopic);

      char downSignalTopic[50];
      sprintf(downSignalTopic, "owlcms/fop/down/#", fop);
      mqttClient.subscribe(downSignalTopic);

      char breakTopic[50];
      sprintf(breakTopic, "owlcms/fop/break/#", fop);
      mqttClient.subscribe(breakTopic);

      char juryDeliberationTopic[50];
      sprintf(juryDeliberationTopic, "owlcms/fop/juryDeliberation/#", fop);
      mqttClient.subscribe(juryDeliberationTopic);

      char juryChallengeTopic[50];
      sprintf(juryChallengeTopic, "owlcms/fop/challenge/#", fop);
      mqttClient.subscribe(juryChallengeTopic);

      char endBreakTopic[50];
      sprintf(endBreakTopic, "owlcms/fop/startLifting/#", fop);
      mqttClient.subscribe(endBreakTopic);

      updateDisplay(true, true, referee, getBatteryPercentage(), "", "");
    } else {
      Serial.print("MQTT connection failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 second");
      delay(1000);
    }
  }
}