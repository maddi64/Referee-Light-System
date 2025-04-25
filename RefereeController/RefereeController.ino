// ====== START CONFIG SECTION ======================================================
#include <Arduino.h>
#include <EEPROM.h>
#include "battery.h"
#include "connections.h"
//#include "decision.h"

#define EEPROM_SIZE 1

//______Allocate Pins___________________________________________
int decisionPins[] = {14, 27};
int ledPins[] = {15};
int hapticPins[] = {4, 18};
int batteryPins[] = {16, 17, 22, 23};

#define ELEMENTCOUNT(x)  (sizeof(x) / sizeof(x[0]))

// ====== END CONFIG SECTION ======================================================

// ====== Globals ======================================================
int referee = 1;
const char* platform = "A";
char fop[20];

int ref13Number = 0;
int prevDecisionPinState[] = {-1, -1, -1, -1, -1, -1};
int ledStartedMillis[] = {0, 0, 0};
int ledDuration[] = {0, 0, 0};

bool refSet = false;

// ====== Function Prototypes ======================================================
void setRefNumber();
void setupPins();
void buttonLoop();
void ledLoop();
void sendDecision(int ref02Number, const char* decision);
void changeReminderStatus(int ref13Number, boolean warn);
void changeSummonStatus(int ref02Number, boolean warn);
void callback(char* topic, byte* message, unsigned int length);

// ====== Function Definitions ======================================================

void setRefNumber() {
  Serial.println("Set Ref Mode Initiated");
  if (referee > 1) {
    for (int i = 0; i < referee; i++) {
      analogWrite(batteryPins[i], 100);
    }
  } else { 
    analogWrite(batteryPins[0], 100);
  }
  
  // Add timestamp for timeout
  unsigned long startTime = millis();
  const unsigned long timeout = 15000; // 15 seconds in milliseconds
  
  while (!refSet) {
    bool buttonA = digitalRead(decisionPins[0]) == LOW;
    bool buttonB = digitalRead(decisionPins[1]) == LOW;
    
    // Check if timeout has elapsed
    if (millis() - startTime >= timeout) {
      Serial.println("Timeout: Exiting function automatically after 15 seconds.");
      Serial.print("Referee set to: ");
      Serial.println(referee);
      EEPROM.write(0, referee);
      EEPROM.commit();
      refSet = true;
      return;
    }

    // Exit condition: button B pressed
    if (buttonB) {
      Serial.print("Exiting function. Referee set to: ");
      Serial.println(referee);
      EEPROM.write(0, referee);
      EEPROM.commit();
      refSet = true;
      return;
    }

    // Increment condition: only button A pressed
    if (buttonA) {
      delay(200); // Simple debounce
      while (digitalRead(decisionPins[0]) == LOW); // Wait for release
      
      // Reset the timeout timer when user interacts
      startTime = millis();

      if (referee < 3) {
        analogWrite(batteryPins[referee], 100);
        Serial.println(referee);
        referee++;
      } else {
        // Reset all except LED 0
        for (int i = 1; i < 3; i++) {
          analogWrite(batteryPins[i], 0);
        }
        referee = 1; // Reset count (LED 0 stays on)
        Serial.println("Cycle complete. LEDs reset.");
      }
    }
  }
}

void buttonLoop() {
  for (int j = 0; j < ELEMENTCOUNT(decisionPins); j++) {
    int state = digitalRead(decisionPins[j]);
    int prevState = prevDecisionPinState[j];
    if (state != prevState) {
      prevDecisionPinState[j] = state;
      if (state == LOW) {
        if (j % 2 == 0) {
          sendDecision(j / 2, "good");
        } else {
          sendDecision(j / 2, "bad");
        }
        analogWrite(ledPins[0], 0);
        digitalWrite(hapticPins[0], LOW);
        digitalWrite(hapticPins[1], LOW);
        return;
      }
    }
  }
}

void ledLoop() {
  for (int j = 0; j < ELEMENTCOUNT(ledStartedMillis); j++) {
    if (ledStartedMillis[j] > 0) {
      if (millis() - ledStartedMillis[j] >= ledDuration[j]) {
        analogWrite(ledPins[0], 0);
        digitalWrite(hapticPins[j], LOW);
        ledStartedMillis[j] = 0;
      }
    }
  }
}

void sendDecision(int ref02Number, const char* decision) {
  if (referee > 0) {
    ref02Number = referee - 1;
  }

  char topic[50];
  sprintf(topic, "owlcms/decision/%s", fop);
  char message[32];
  sprintf(message, "%i %s", ref02Number + 1, decision);

  mqttClient.publish(topic, message);
  Serial.print(topic); Serial.print(" "); Serial.print(message); Serial.println(" sent.");
}

void changeReminderStatus(int ref13Number, boolean warn) {
  Serial.print("reminder "); Serial.print(warn); Serial.print(" "); Serial.println(ref13Number);
  if (ref13Number == referee) {
    if (warn) {
      analogWrite(ledPins[0], 255);
      digitalWrite(hapticPins[0], HIGH);
      digitalWrite(hapticPins[1], HIGH);
    } else {
      analogWrite(ledPins[0], 0);
      digitalWrite(hapticPins[0], LOW);
      digitalWrite(hapticPins[1], LOW);
    }
  }
}

void changeSummonStatus(int ref02Number, boolean warn) {
  Serial.print("summon "); Serial.print(warn); Serial.print(" "); Serial.println(ref02Number + 1);
  if (warn) {
    analogWrite(ledPins[0], 255);
    digitalWrite(hapticPins[0], HIGH);
    digitalWrite(hapticPins[1], HIGH);
  } else {
    analogWrite(ledPins[0], 0);
    digitalWrite(hapticPins[0], LOW);
    digitalWrite(hapticPins[1], LOW);
  }
}

void setupPins() {
  for (int j = 0; j < ELEMENTCOUNT(decisionPins); j++) {
    pinMode(decisionPins[j], INPUT_PULLUP);
    prevDecisionPinState[j] = digitalRead(decisionPins[j]);
  }

  pinMode(ledPins[0], OUTPUT);

  for (int j = 0; j < ELEMENTCOUNT(hapticPins); j++) {
    pinMode(hapticPins[j], OUTPUT);
    digitalWrite(hapticPins[j], LOW);
  }
}

// ====== MQTT Callback ======================================================

String decisionRequestTopic = String("owlcms/decisionRequest/") + fop;
String summonTopic = String("owlcms/summon/") + fop;
String ledTopic = String("owlcms/led/") + fop;
String resetTopic = String("owlcms/reset/");

void callback(char* topic, byte* message, unsigned int length) {
  String stTopic = String(topic);
  Serial.print("Message arrived on topic: "); Serial.print(stTopic); Serial.print("; Message: ");

  String stMessage;
  for (int i = 0; i < length; i++) {
    stMessage += (char)message[i];
  }

  int refIndex = stTopic.lastIndexOf("/") + 1;
  String refString = stTopic.substring(refIndex);
  int ref13Number = refString.toInt();

  if (stTopic.startsWith(decisionRequestTopic)) {
    changeReminderStatus(ref13Number, stMessage.startsWith("on"));
  } else if (stTopic.startsWith(summonTopic)) {
    if (ref13Number == 0) {
      for (int j = 0; j < ELEMENTCOUNT(ledPins); j++) {
        changeSummonStatus(j, stMessage.startsWith("on"));
      }
    } else {
      changeSummonStatus(ref13Number, stMessage.startsWith("on"));
    }
  } else if (stTopic.startsWith(ledTopic)) {
    if (ref13Number == 0) {
      for (int j = 0; j < ELEMENTCOUNT(ledPins); j++) {
        changeSummonStatus(j, stMessage.startsWith("on"));
      }
    } else {
      changeSummonStatus(ref13Number - 1, stMessage.startsWith("on"));
    }
  } else if (stTopic.startsWith(resetTopic)) {
    analogWrite(ledPins[0], 0);
  }
}

// ====== Setup and Loop ======================================================

void setup() {
  EEPROM.begin(EEPROM_SIZE);
  if (EEPROM.read(0) == NULL) {
    referee = 1; 
  } else {
    referee = EEPROM.read(0);
  }
  setupBatteryPins();
  setupPins();
  setRefNumber();
  Serial.print(referee);
  xTaskCreatePinnedToCore(
    batteryMonitoringTask,
    "Battery Monitor",
    2048,
    NULL,
    1,
    NULL,
    1
  );
  setupConnections();
}

void loop() {
  delay(10);
  if (!mqttClient.connected()) {
    mqttReconnect();
  }
  mqttClient.loop();
  buttonLoop();
  ledLoop();
}