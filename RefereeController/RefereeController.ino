// ====== START CONFIG SECTION ======================================================
#include <Arduino.h>
#include <EEPROM.h>
#include <U8g2lib.h>
#include <Wire.h>
// #include <SPI.h>
#include "connections.h"
#include "battery.h"
#include "interface.h"

#define EEPROM_SIZE 1

//______Allocate Pins___________________________________________
int decisionPins[] = {14, 27};
int hapticPins[] = {4, 18};

// OLED Display pins
// #define OLED_CS 5     // Chip select
// #define OLED_DC 17    // Data/Command
// #define OLED_RESET 16 // Reset

#define SDA_PIN 17
#define SCL_PIN 5

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R0,         // Rotation
  /* reset=*/ U8X8_PIN_NONE, 
  /* clock=*/ SCL_PIN, 
  /* data=*/ SDA_PIN
);


// Initialize display with hardware SPI
// U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI u8g2(
//   U8G2_R0,        // No rotation
//   /* cs=*/ OLED_CS,     // Chip Select pin
//   /* dc=*/ OLED_DC,    // Data/Command pin
//   /* reset=*/ OLED_RESET  // Reset pin
// );


#define ELEMENTCOUNT(x)  (sizeof(x) / sizeof(x[0]))

// ====== END CONFIG SECTION ======================================================

// ====== Globals ======================================================
int referee = 1;
const char* platform = "A";
char fop[20];

int ref13Number = 0;
int prevDecisionPinState[] = {-1, -1};
bool refSet = false;

String currentDecision = "";
String warningMessage = "";
bool lowBattery = false;
bool previousBatteryState = true;


// ====== Function Prototypes ======================================================
void setRefNumber();
void setupPins();
void buttonLoop();
void sendDecision(int ref02Number, const char* decision);
void changeReminderStatus(int ref13Number, boolean warn);
void changeSummonStatus(int ref02Number, boolean warn);
void callback(char* topic, byte* message, unsigned int length);

// ====== Function Definitions ======================================================

void setRefNumber() {
  Serial.println("Set Ref Mode Initiated");
  warningMessage = "SELECT REFEREE NUMBER";
  unsigned long startTime = millis();
  const unsigned long timeout = 15000; 
  unsigned long lastUpdateTime = 0;
  int lastSecondsRemaining = 15;

  refSelectionDispay(referee, lastSecondsRemaining);
  
  
  
  while (!refSet) {
    bool buttonA = digitalRead(decisionPins[0]) == LOW;
    bool buttonB = digitalRead(decisionPins[1]) == LOW;

    unsigned long currentTime = millis();
    unsigned long elapsed = currentTime - startTime;
    
    // Timeout handling
    if (elapsed >= timeout) {
      Serial.println("Timeout: Exiting function automatically");
      Serial.print("Referee set to: ");
      Serial.println(referee);
      EEPROM.write(0, referee);
      EEPROM.commit();
      refSet = true;
      warningMessage = "";
      refSelectionDispay(referee, 0);
      return;
    }

    int secondsRemaining = 15 - (elapsed / 1000);
    if (secondsRemaining != lastSecondsRemaining) {
      lastSecondsRemaining = secondsRemaining;
      refSelectionDispay(referee, secondsRemaining);
    }


    if (buttonB) {
      Serial.println("Exiting function. Referee set to: ");
      Serial.println(referee);
      EEPROM.write(0, referee);
      EEPROM.commit();
      refSet = true;
      warningMessage = "";
      refSelectionDispay(referee, secondsRemaining);
      return;
    }

    if (buttonA) {
      delay(200); // Debounce
      while (digitalRead(decisionPins[0]) == LOW); // Wait for release
      referee = (referee % 3) + 1;
      refSelectionDispay(referee, secondsRemaining);
      Serial.println(referee);
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
          currentDecision = "GOOD LIFT";
        } else {
          sendDecision(j / 2, "bad");
          currentDecision = "BAD LIFT";
        }
        updateDisplay(WiFi.status() == WL_CONNECTED, mqttClient.connected(), referee, getBatteryPercentage(), currentDecision, warningMessage);
        digitalWrite(hapticPins[0], LOW);
        digitalWrite(hapticPins[1], LOW);
        return;
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
      warningMessage = "MAKE DECISION";
      digitalWrite(hapticPins[0], HIGH);
      digitalWrite(hapticPins[1], HIGH);
    } else {
      warningMessage = "";
      digitalWrite(hapticPins[0], LOW);
      digitalWrite(hapticPins[1], LOW);
    }
    updateDisplay(WiFi.status() == WL_CONNECTED, mqttClient.connected(), referee, getBatteryPercentage(), currentDecision, warningMessage);
  }
}

void changeSummonStatus(int ref02Number, boolean warn) {
  if (warn) {
    warningMessage = "SUMMONED BY JURY";
    digitalWrite(hapticPins[0], HIGH);
    digitalWrite(hapticPins[1], HIGH);
    updateDisplay(WiFi.status() == WL_CONNECTED, mqttClient.connected(), referee, getBatteryPercentage(), currentDecision, warningMessage);
    
    // Start timer to clear summon after 10 seconds
    delay(10000);
    warningMessage = "";
    digitalWrite(hapticPins[0], LOW);
    digitalWrite(hapticPins[1], LOW);
    updateDisplay(WiFi.status() == WL_CONNECTED, mqttClient.connected(), referee, getBatteryPercentage(), currentDecision, warningMessage);
  }
}

void setupPins() {
  for (int j = 0; j < ELEMENTCOUNT(decisionPins); j++) {
    pinMode(decisionPins[j], INPUT_PULLUP);
    prevDecisionPinState[j] = digitalRead(decisionPins[j]);
  }

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
String downSignalTopic = String("owlcms/fop/down/" + String(fop));

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
      changeSummonStatus(0, stMessage.startsWith("on"));
    } else {
      changeSummonStatus(ref13Number - 1, stMessage.startsWith("on"));
    }
  } else if (stTopic.startsWith(resetTopic)) {
    currentDecision = "";
    warningMessage = "";
    updateDisplay(WiFi.status() == WL_CONNECTED, mqttClient.connected(), referee, getBatteryPercentage(), "", "");
  } else if (stTopic.startsWith(downSignalTopic)) {
    currentDecision = "";
    warningMessage = "";
    updateDisplay(WiFi.status() == WL_CONNECTED, mqttClient.connected(), referee, getBatteryPercentage(), "", "");
  }
}

void noWarning() {
  warningMessage = "";
  updateDisplay(WiFi.status() == WL_CONNECTED, mqttClient.connected(), referee, getBatteryPercentage(), "", warningMessage);
}

void lowBatteryCheck() {
  //updateDisplay(WiFi.status() == WL_CONNECTED, mqttClient.connected(), referee, getBatteryPercentage(), "", "BATTERY LOW. PLEASE CHARGE.");
  if (lowBattery == true) {
    warningMessage = "BATTERY LOW. PLEASE CHARGE.";
  } else {
    warningMessage = "";
  }
  //Serial.println("The display should have updated");
}

// ====== Setup and Loop ======================================================

void setup() {
  Serial.begin(115200);
  
  // Initialize OLED display
  Wire.begin(SDA_PIN, SCL_PIN);
  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.clearDisplay();
  delay(100);  // Give display time to initialize
  u8g2.setPowerSave(0);
  u8g2.setContrast(255); // Maximum contrast
  u8g2.clearBuffer();

  // u8g2.drawXBMP(3, 2, 19, 16, image_wifi_full_bits);
  // u8g2.drawXBMP(3, 2, 19, 16, image_wifi_full_bits);
  // u8g2.drawXBMP(104, 1, 24, 16, image_battery_empty_bits);
  // u8g2.setFont(u8g2_font_profont22_tr);
  // int textWidth = u8g2.getStrWidth("TEST");
  // int x = (127 - textWidth) / 2;
  // u8g2.drawStr(x, 41, "TEST");

  // Initial display update
  updateDisplay(false, false, referee, getBatteryPercentage(), "", "Initializing...");

  u8g2.sendBuffer();
  
  EEPROM.begin(EEPROM_SIZE);
  if (EEPROM.read(0) == 255) {
    referee = 1;
  } else {
    referee = EEPROM.read(0);
  }
  
  setupBatteryPins();
  setupPins();
  
  
  setRefNumber();
  
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
    updateDisplay(WiFi.status() == WL_CONNECTED, false, referee, getBatteryPercentage(), currentDecision, "Server connection lost");
  }
  mqttClient.loop();
  buttonLoop();
  //checkBatteryLevel();

  lowBatteryCheck();
  
  // Update display status periodically
  static unsigned long lastDisplayUpdate = 0;
  if (millis() - lastDisplayUpdate >= 1000) {
    updateDisplay(WiFi.status() == WL_CONNECTED, mqttClient.connected(), referee, getBatteryPercentage(), currentDecision, warningMessage);
    lastDisplayUpdate = millis();
  }
}