// ====== START CONFIG SECTION ======================================================
#include "secrets_local.h"
#include <driver/ledc.h>
#include <Arduino.h>
#include <Ticker.h>

const char* platform = "A";
char fop[20];

String downSignalTopic;
String decisionTopic;
String resetDecisionsTopic;
String timeRemainingTopic;

String ref1Decision = "";
String ref2Decision = "";
String ref3Decision = "";

bool downTriggered = false;
bool allDecisionsMade = false;
bool silentMode = false; 

int retryCounter = 0;

Ticker buzzerTimer;

// Variables for non-blocking timing of down signal
unsigned long downSignalStartTime = 0;
bool buzzerOn = false;
bool downLedOn = false;

//______Allocate Pins___________________________________________

const int downLedPin = 15;
const int buzzerPin = 16;
const int refGoodDecisions[] = {17, 18, 19};
const int refBadDecisions[] = {21, 22, 23}; 


// ====== END CONFIG SECTION ======================================================

#ifdef TLS
#include <WiFiClientSecure.h>
#include "certificates.h"
const int mqttPort = 8883;
#else
#include <WiFi.h>
const int mqttPort = 1883;
#endif

#include "PubSubClient.h"
#define ELEMENTCOUNT(x) (sizeof(x) / sizeof(x[0]))

#ifdef TLS
WiFiClientSecure wifiClient;
#else
WiFiClient wifiClient;
#endif
PubSubClient mqttClient;

// networking values
String macAddress;
char mac[50];
char clientId[50];

unsigned long decisionStartTime = 0;
unsigned long ignoreStartTime = 0;
bool isWaitingForDecision = false;
bool isIgnoringDecisions = false;

void setup() {
  digitalWrite(downLedPin, HIGH);
  setupPins();
  bootSequence();
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
  downSignalTopic = "owlcms/fop/down/" + String(fop);
  decisionTopic = "owlcms/decision/" + String(fop);
  resetDecisionsTopic = "owlcms/fop/resetDecisions/" + String(fop);
  timeRemainingTopic = "owlcms/fop/timeRemaining/" + String(fop);
  mqttReconnect();
  digitalWrite(downLedPin, LOW);
}

void loop() {
  delay(10);
  if (!mqttClient.connected()) {
    mqttReconnect();
  }
  mqttClient.loop();
  //silentMode();

  unsigned long currentTime = millis();

  // Handle down signal timing (non-blocking)
  if (buzzerOn && (currentTime - downSignalStartTime >= 1500)) {
    // Turn off buzzer after 1.5 seconds
    digitalWrite(buzzerPin, LOW);
    buzzerOn = false;
    Serial.println("Buzzer turned off");
  }
  
  if (downLedOn && (currentTime - downSignalStartTime >= 3000)) {
    // Turn off down LED after 3 seconds
    digitalWrite(downLedPin, LOW);
    downLedOn = false;
    Serial.println("Down LED turned off");
  }

  // Handle decision timing
  if (isWaitingForDecision && (currentTime - decisionStartTime >= 3000)) {
    isWaitingForDecision = false;
    setDecisionLights();
    isIgnoringDecisions = true;
    ignoreStartTime = currentTime;
  }

  if (isIgnoringDecisions && (currentTime - ignoreStartTime >= 5000)) {
    isIgnoringDecisions = false;
  }
}

void wifiConnect() {
  Serial.print("Connecting to WiFi ");
  Serial.print(wifiSSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPassword);
  while (WiFi.status() != WL_CONNECTED) {
    disconnectLEDs();
    delay(500);
    Serial.print(".");
    retryCounter++;
    if (retryCounter == 3) {
      ESP.restart();
    }
  }
  retryCounter = 0;
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
      mqttClient.subscribe(downSignalTopic.c_str());
      mqttClient.subscribe(decisionTopic.c_str());
      mqttClient.subscribe(resetDecisionsTopic.c_str());
      mqttClient.subscribe(timeRemainingTopic.c_str());
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
    analogWrite(refBadDecisions[0], dutyCycle);
    analogWrite(refBadDecisions[1], dutyCycle);
    analogWrite(refBadDecisions[2], dutyCycle);
    delay(10);
  }

  for(int dutyCycle = 255; dutyCycle >= 0; dutyCycle--){
    analogWrite(refBadDecisions[0], dutyCycle);
    analogWrite(refBadDecisions[1], dutyCycle);
    analogWrite(refBadDecisions[2], dutyCycle);
    delay(10);
  }
}

void bootSequence() {
  for(int dutyCycle = 0; dutyCycle <= 255; dutyCycle++){   
    analogWrite(refBadDecisions[0], dutyCycle);
    delay(1);
    analogWrite(refBadDecisions[1], dutyCycle);
    delay(1);
    analogWrite(refBadDecisions[2], dutyCycle);
    delay(3);
  }

  for(int dutyCycle = 255; dutyCycle >= 0; dutyCycle--){
    analogWrite(refBadDecisions[0], dutyCycle);
    delay(1);
    analogWrite(refBadDecisions[1], dutyCycle);
    delay(1);
    analogWrite(refBadDecisions[2], dutyCycle);
    delay(3);
  }
}

void setupPins() {
  pinMode(downLedPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  
  //pinMode(silentModePin, INPUT_PULLUP);
  
  // for (int i = 0; i < 3; i++) {
  //   pinMode(decisionPins[i], OUTPUT);
  // }

  for (int i = 0; i < 3; i++) {
    pinMode(refGoodDecisions[i], OUTPUT);
    pinMode(refBadDecisions[i], OUTPUT);
  }
}

void callback(char* topic, byte* message, unsigned int length) {
  String stTopic = String(topic);
  String stMessage;

  for (int i = 0; i < length; i++) {
    stMessage += (char)message[i];
  }

  if (stTopic.startsWith(decisionTopic)) {
    if (!isIgnoringDecisions) {
      processDecision(stMessage);
      Serial.println(stTopic + stMessage);
    } else {
      Serial.println("Ignoring decision");
    }
  }

  if (stTopic.startsWith(resetDecisionsTopic)) {
    resetDecisions();
  }

  if (stTopic.startsWith(downSignalTopic)) {
    Serial.println(stTopic + stMessage);
    downSignal();
  }

  if (stTopic.startsWith(timeRemainingTopic)) {
    timeRemainingBuzzer(stMessage);
  }
}

void processDecision(String message) {
  int spaceIndex = message.indexOf(' ');
  if (spaceIndex == -1) {
    Serial.println("Invalid message format");
    return;
  }

  String refNumber = message.substring(0, spaceIndex);
  String decision = message.substring(spaceIndex + 1);

  if (decision != "good" && decision != "bad") {
    Serial.println("Invalid decision in message");
    return;
  }

  if (refNumber == "1") {
    ref1Decision = decision;
  } else if (refNumber == "2") {
    ref2Decision = decision;
  } else if (refNumber == "3") {
    ref3Decision = decision;
  } else {
    Serial.println("Invalid referee number in message");
    return;
  }

  //checkDecisions();
}

// void checkDecisions() {
//   //int goodCount = 0, badCount = 0;
//   String decisions[] = {ref1Decision, ref2Decision, ref3Decision};

//   // for (String d : decisions) {
//   //   if (d == "good") goodCount++;
//   //   else if (d == "bad") badCount++;
//   // }

//   // if (!downTriggered && (goodCount == 2 || badCount == 2)) {
//   //   downSignal();
//   //   downTriggered = true;
//   // }

//   if (!ref1Decision.isEmpty() && !ref2Decision.isEmpty() && !ref3Decision.isEmpty()) {
//     setDecisionLights();
//     resetDecisions();
//   }
// }

void setDecisionLights() {
  digitalWrite(buzzerPin, LOW);
  digitalWrite(downLedPin, LOW);
  
  analogWrite(refGoodDecisions[0], ref1Decision == "good" ? 255 : 0);
  analogWrite(refBadDecisions[0], ref1Decision == "bad" ? 255 : 0);

  analogWrite(refGoodDecisions[1], ref2Decision == "good" ? 255 : 0);
  analogWrite(refBadDecisions[1], ref2Decision == "bad" ? 255 : 0);

  analogWrite(refGoodDecisions[2], ref3Decision == "good" ? 255 : 0);
  analogWrite(refBadDecisions[2], ref3Decision == "bad" ? 255 : 0);
   
  delay(3000);
  for (int i = 0; i < 3; i++) {
    analogWrite(refBadDecisions[i], 0);
    analogWrite(refGoodDecisions[i], 0);
  }

  resetDecisions();
}

void downSignal() {
  // Turn on both down LED and buzzer
  digitalWrite(downLedPin, HIGH);
  digitalWrite(buzzerPin, HIGH);
  
  // Record the time when down signal was triggered
  downSignalStartTime = millis();
  
  // Set flags to track states
  buzzerOn = true;
  downLedOn = true;
  
  // Log the action
  Serial.println("Down signal activated - buzzer and LED ON");

  if (!isWaitingForDecision && !isIgnoringDecisions) {
    Serial.println("it reaches here");
    isWaitingForDecision = true;
    decisionStartTime = millis();
  }
  // checkDecisions();
}

void resetDecisions() {
  ref1Decision = "";
  ref2Decision = "";  
  ref3Decision = "";
  downTriggered = false;
  Serial.println(ref1Decision + ref2Decision + ref3Decision);
}

void turnOffBuzzer() {
  digitalWrite(buzzerPin, LOW);
  digitalWrite(downLedPin, LOW);
}

void timeRemainingBuzzer(String message) {
  String timeRemaining = message;

  if (message == "90" || message == "30") {
    digitalWrite(buzzerPin, HIGH);
    buzzerTimer.once(0.2, turnOffBuzzer); 
  }

  if (message == "0") {
    digitalWrite(buzzerPin, HIGH);
    digitalWrite(downLedPin, HIGH);
    buzzerTimer.once(1.5, turnOffBuzzer);
  }
}