// ====== START CONFIG SECTION ======================================================
#include <Arduino.h>
#include <esp_system.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <EEPROM.h>
#include <U8g2lib.h>
#include <Wire.h>
// #include <SPI.h>
#include "connections.h"
#include "battery.h"
#include "interface.h"

#define EEPROM_SIZE 8  // Increased to 8: byte 0 for referee, byte 1 for restart flag, bytes 2-5 for lowest voltage

//______Allocate Pins___________________________________________
int decisionPins[] = {14, 27};
int hapticPins[] = {4, 18};

// OLED Display pins
// #define OLED_CS 5     // Chip select
// #define OLED_DC 17    // Data/Command
// #define OLED_RESET 16 // Reset

#define SDA_PIN 17
#define SCL_PIN 5
#define DISPLAY_POWER_PIN 19  // Control display power/ground - Not used

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R0,         // Rotation
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
String reminderMessage = "";
bool lowBattery = false;
bool previousBatteryState = true;

// Timer for clearing decision display after downSignal
unsigned long downSignalTime = 0;
bool downSignalReceived = false;

bool inBreak = false;

// ====== Function Prototypes ======================================================
void setRefNumber();
void setupPins();
void buttonLoop();
void sendDecision(int ref02Number, const char* decision);
void changeReminderStatus(int ref13Number, boolean warn);
void changeSummonStatus(int ref02Number, boolean warn);
void assignBreak(String stMessage);
void callback(char* topic, byte* message, unsigned int length);
void updateDisplayWithPriority(bool wifi_status, bool mqtt_status, int referee, int battery_percent, const String& mainText, const String& warning, const String& reminder);
bool checkOTABootCondition();
void switchToOTAPartition();
void startOTAWebServer();
void handleOTAUpload();
void handleOTAUploadFinish();

// ====== Function Definitions ======================================================

void setRefNumber() {
  Serial.println("Set Ref Mode Initiated");
  
  // Power up the display properly - DISPLAY_POWER_PIN not used
  digitalWrite(DISPLAY_POWER_PIN, LOW);  // Connect to ground
  delay(500);  // Wait for display to stabilize
  
  // Re-initialize I2C and display after power up
  Wire.begin(SDA_PIN, SCL_PIN);
  //delay(50);
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.sendBuffer();
  delay(200);
  u8g2.clearBuffer();
  // u8g2.setDrawColor(0); // Set color to black
  // u8g2.drawBox(0, 0, u8g2.getDisplayWidth(), u8g2.getDisplayHeight());
  u8g2.sendBuffer();
  
  
  
  // Load referee from EEPROM and display immediately
  // EEPROM already initialized in setup(), no need to call begin() again
  if (EEPROM.read(0) == 255) {
    referee = 1;
  } else {
    referee = EEPROM.read(0);
  }
  
  // Show the current referee number from EEPROM immediately
  refSelectionDispay(referee, 15);
  
  unsigned long startTime = millis();
  const unsigned long timeout = 15000; 
  unsigned long lastUpdateTime = 0;
  int lastSecondsRemaining = 15;
  
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
      if (inBreak == false) {
        prevDecisionPinState[j] = state;
        if (state == LOW) {
          if (j % 2 == 0) {
            sendDecision(j / 2, "good");
            currentDecision = "GOOD LIFT";
          } else {
            sendDecision(j / 2, "bad");
            currentDecision = "BAD LIFT";
          }
          
          // Clear reminder message when a decision is made
          reminderMessage = "";
          
          updateDisplayWithPriority(WiFi.status() == WL_CONNECTED, mqttClient.connected(), referee, getBatteryPercentage(), currentDecision, warningMessage, reminderMessage);
          digitalWrite(hapticPins[0], LOW);
          digitalWrite(hapticPins[1], LOW);
          return;
        }
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
      reminderMessage = "Required for down signal";
      currentDecision = "MAKE DECISION";
      digitalWrite(hapticPins[0], HIGH);
      digitalWrite(hapticPins[1], HIGH);
    } else {
      reminderMessage = "";
      digitalWrite(hapticPins[0], LOW);
      digitalWrite(hapticPins[1], LOW);
    }
    updateDisplayWithPriority(WiFi.status() == WL_CONNECTED, mqttClient.connected(), referee, getBatteryPercentage(), currentDecision, warningMessage, reminderMessage);
  }
}

void changeSummonStatus(int ref02Number, boolean warn) {
  if (ref02Number == referee || ref02Number == 0) {
    if (warn) {
      currentDecision = "SUMMONED";
      reminderMessage = "Please see the Jury";
      digitalWrite(hapticPins[0], HIGH);
      digitalWrite(hapticPins[1], HIGH);
      updateDisplayWithPriority(WiFi.status() == WL_CONNECTED, mqttClient.connected(), referee, getBatteryPercentage(), currentDecision, warningMessage, reminderMessage);
      
      // Start timer to clear summon after 10 seconds
      delay(5000);
      currentDecision = "";
      reminderMessage = "";
      digitalWrite(hapticPins[0], LOW);
      digitalWrite(hapticPins[1], LOW);
      updateDisplayWithPriority(WiFi.status() == WL_CONNECTED, mqttClient.connected(), referee, getBatteryPercentage(), currentDecision, warningMessage, reminderMessage);
    }
  }
}


void assignBreak(String stMessage) {
  inBreak = true;
  if(stMessage == "BEFORE_INTRODUCTION") {
    warningMessage = "Break before introduction";
    currentDecision = "BREAK";
  } else if (stMessage == "FIRST_SNATCH") {
    warningMessage = "Break before snatch";
    currentDecision = "BREAK";
  } else if (stMessage == "FIRST_CJ") {
    warningMessage = "Break before CJ";
    currentDecision = "BREAK";
  } else if (stMessage == "GROUP_DONE") {
    currentDecision = "SESSION DONE";
  } else if (stMessage == "TECHNICAL") {
    currentDecision = "TECHNICAL BREAK";
  } else if (stMessage == "MARSHAL") {
    currentDecision = "MARSHAL BREAK";
  } else if (stMessage == "CEREMONY") {
    currentDecision = "CEREMONY";
  }

  updateDisplayWithPriority(WiFi.status() == WL_CONNECTED, mqttClient.connected(), referee, getBatteryPercentage(), currentDecision, warningMessage, reminderMessage);
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

// Priority display function: reminder messages take priority over warning messages
void updateDisplayWithPriority(bool wifi_status, bool mqtt_status, int referee, int battery_percent, const String& mainText, const String& warning, const String& reminder) {
  String displayMessage = reminder.length() > 0 ? reminder : warning;
  updateDisplay(wifi_status, mqtt_status, referee, battery_percent, mainText, displayMessage);
}

// ====== MQTT Callback ======================================================

String decisionRequestTopic = String("owlcms/decisionRequest/") + fop;
String summonTopic = String("owlcms/summon/") + fop;
String ledTopic = String("owlcms/led/") + fop;
String resetTopic = String("owlcms/fop/resetDecisions/" + String(fop));
String downSignalTopic = String("owlcms/fop/down/" + String(fop));
String breakTopic = String("owlcms/fop/break/") + fop;
String juryDeliberationTopic = String("owlcms/fop/juryDeliberation/") + fop;
String juryChallengeTopic = String("owlcms/fop/challenge/") + fop;
String endBreakTopic = String("owlcms/fop/startLifting/") + fop;

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
      changeSummonStatus(ref13Number, stMessage.startsWith("on"));
    }
  } else if (stTopic.startsWith(resetTopic)) {
    currentDecision = "";
    reminderMessage = "";
    updateDisplayWithPriority(WiFi.status() == WL_CONNECTED, mqttClient.connected(), referee, getBatteryPercentage(), "", warningMessage, reminderMessage);
  } else if (stTopic.startsWith(downSignalTopic)) {
    // Start 3-second timer to clear decision display
    downSignalTime = millis();
    downSignalReceived = true;
  } else if (stTopic.startsWith(breakTopic)) {
    Serial.println(stMessage);
    assignBreak(stMessage);
  } else if (stTopic.startsWith(juryDeliberationTopic)) {
    Serial.println(stMessage);
    inBreak = true; 
    currentDecision = "DELIBERATION";
    updateDisplayWithPriority(WiFi.status() == WL_CONNECTED, mqttClient.connected(), referee, getBatteryPercentage(), currentDecision, warningMessage, reminderMessage);
  } else if (stTopic.startsWith(juryChallengeTopic)) {
    Serial.println(stMessage);
    inBreak = true; 
    currentDecision = "CHALLENGE";
    updateDisplayWithPriority(WiFi.status() == WL_CONNECTED, mqttClient.connected(), referee, getBatteryPercentage(), currentDecision, warningMessage, reminderMessage);
  } else if (stTopic.startsWith(endBreakTopic)) {
    Serial.println(stMessage);
    currentDecision = "";
    warningMessage = "";
    inBreak = false; 
    updateDisplayWithPriority(WiFi.status() == WL_CONNECTED, mqttClient.connected(), referee, getBatteryPercentage(), currentDecision, warningMessage, reminderMessage);
  }
}

void noWarning() {
  reminderMessage = "";
  updateDisplayWithPriority(WiFi.status() == WL_CONNECTED, mqttClient.connected(), referee, getBatteryPercentage(), "", warningMessage, reminderMessage);
}

bool checkOTABootCondition() {
  Serial.println("Checking for OTA boot condition (A+B buttons held for 5 seconds)...");
  
  // Check if both buttons are pressed initially
  if (digitalRead(decisionPins[0]) != LOW || digitalRead(decisionPins[1]) != LOW) {
    return false;  // One or both buttons not pressed
  }
  
  unsigned long startTime = millis();
  const unsigned long holdDuration = 5000;  // 5 seconds
  
  while (millis() - startTime < holdDuration) {
    // Check if either button is released
    if (digitalRead(decisionPins[0]) != LOW || digitalRead(decisionPins[1]) != LOW) {
      Serial.println("Button(s) released before 5 seconds - normal boot");
      return false;
    }
    delay(50);  // Small delay to prevent excessive polling
  }
  
  Serial.println("Both buttons held for 5 seconds - switching to OTA partition");
  return true;
}

WebServer otaServer(80);

void startOTAWebServer() {
  Serial.println("Starting OTA Web Server...");
  
  // Connect to WiFi (reuse existing credentials)
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPassword);  // Use existing WiFi credentials from connections.cpp

  drawOTAWaitingScreen();
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Failed to connect to WiFi");
    drawOTAErrorScreen("WiFi connection failed");
    delay(5000);
    esp_restart();
    return;
  }
  
  Serial.println("");
  Serial.print("WiFi connected. IP address: ");
  Serial.println(WiFi.localIP());
  
  // Show IP address on display
  drawOTAReadyScreen(WiFi.localIP().toString());
  
  // Set up web server routes
  otaServer.on("/", HTTP_GET, []() {
    String html = "<!DOCTYPE html><html><head><title>RLSX2 Controller Update</title></head><body>";
    html += "<h1>RLSX2 Controller Update</h1>";
    html += "<form method='POST' action='/update' enctype='multipart/form-data'>";
    html += "<input type='file' name='firmware' accept='.bin'><br><br>";
    html += "<input type='submit' value='Upload Firmware'>";
    html += "</form></body></html>";
    otaServer.send(200, "text/html", html);
  });
  
  otaServer.on("/update", HTTP_POST, handleOTAUploadFinish, handleOTAUpload);
  
  otaServer.begin();
  Serial.println("OTA Web Server started");
  
  // Keep server running
  while (true) {
    otaServer.handleClient();
    delay(10);
  }
}

void handleOTAUpload() {
  HTTPUpload& upload = otaServer.upload();
  static int lastProgress = -1;
  
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("Update Start: %s\n", upload.filename.c_str());
    drawOTAUploadingScreen(0);
    
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
      drawOTAErrorScreen("Update begin failed");
      return;
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
      drawOTAErrorScreen("Update write failed");
      return;
    }
    
    // Calculate and display progress
    int progress = (Update.progress() * 100) / Update.size();
    if (progress != lastProgress && progress % 5 == 0) {  // Update display every 5%
      drawOTAUploadingScreen(progress);
      lastProgress = progress;
      Serial.printf("Progress: %d%%\n", progress);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("Update Success: %uB\n", upload.totalSize);
      drawOTACompleteScreen(true, "Upload complete");
    } else {
      Update.printError(Serial);
      drawOTAErrorScreen("Update end failed");
    }
  }
}

void handleOTAUploadFinish() {
  if (Update.hasError()) {
    otaServer.send(500, "text/plain", "Update failed");
    drawOTAErrorScreen("Update failed");
  } else {
    otaServer.send(200, "text/plain", "Update successful! Device will restart.");
    drawOTACompleteScreen(true, "Restarting in 3s");
    delay(3000);
    esp_restart();
  }
}

void switchToOTAPartition() {
  Serial.println("Switching to OTA partition...");
  
  // Initialize display for OTA mode
  digitalWrite(DISPLAY_POWER_PIN, LOW);  // Connect to ground
  delay(500);  // Wait for display to stabilize
  
  // Initialize I2C and display
  Wire.begin(SDA_PIN, SCL_PIN);
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.sendBuffer();
  delay(200);
  u8g2.enableUTF8Print();
  u8g2.setContrast(255);
  
  // Show OTA waiting screen
  drawOTAWaitingScreen();
  delay(2000);  // Show the screen for 2 seconds before switching
  
  // Get the currently running partition
  const esp_partition_t* current_partition = esp_ota_get_running_partition();
  Serial.print("Current partition: ");
  Serial.println(current_partition->label);
  
  // Find the OTA partition (assuming it's the other OTA partition)
  const esp_partition_t* ota_partition = esp_ota_get_next_update_partition(current_partition);
  
  if (ota_partition == NULL) {
    Serial.println("ERROR: No OTA partition found!");
    drawOTAErrorScreen("No OTA partition found");
    delay(3000);
    return;
  }
  
  Serial.print("Switching to partition: ");
  Serial.println(ota_partition->label);
  
  // Show switching screen
  drawOTAProgressScreen(100, "Partition switched");
  delay(2000);
  
  // Set the OTA partition as the boot partition
  esp_err_t err = esp_ota_set_boot_partition(ota_partition);
  if (err != ESP_OK) {
    Serial.print("ERROR: Failed to set boot partition: ");
    Serial.println(esp_err_to_name(err));
    drawOTAErrorScreen("Failed to set boot partition");
    delay(3000);
    return;
  }
  
  Serial.println("Boot partition set successfully. Starting OTA server...");
  
  // Instead of restarting, start the OTA web server
  startOTAWebServer();
}

void lowBatteryCheck() {
  //updateDisplay(WiFi.status() == WL_CONNECTED, mqttClient.connected(), referee, getBatteryPercentage(), "", "BATTERY LOW. PLEASE CHARGE.");
  if (lowBattery == true) {
    warningMessage = "LOW BATTERY. CHARGE NOW";
  } else {
    warningMessage = "";
  }
  //Serial.println("The display should have updated");
}

// ====== Setup and Loop ======================================================

void setup() {
  Serial.begin(115200);
  
  // ESP32 restart on boot for clean initialization (using EEPROM to track)
  EEPROM.begin(EEPROM_SIZE);
  
  // Check if this is the first boot after power-on
  // We use EEPROM address 1 for restart flag (address 0 is used for referee number)
  if (EEPROM.read(1) != 0xAA) {
    // First boot detected, set flag and restart
    EEPROM.write(1, 0xAA);
    EEPROM.commit();
    //Serial.println("Performing ESP32 restart for clean boot...");
    //delay(100);
    esp_restart();
  }
  
  // Clear the restart flag so next power cycle will trigger restart again
  EEPROM.write(1, 0x00);
  EEPROM.commit();
  
  // Configure power control pin and keep display off initially - DISPLAY_POWER_PIN not used
  pinMode(DISPLAY_POWER_PIN, OUTPUT);
  digitalWrite(DISPLAY_POWER_PIN, HIGH);   // Disconnect ground (display off)
  delay(100);  // Ensure display is fully powered down
  
  // Don't initialize display here - it will be done in setRefNumber()
  
  setupBatteryPins();
  setupPins();
  
  // Check for OTA boot condition first (A+B buttons held for 5 seconds)
  if (checkOTABootCondition()) {
    Serial.println("OTA boot condition detected - initializing display and switching partition");
    switchToOTAPartition();
    // This function will restart the ESP32, so code below won't execute
    return;
  }
  
  // Initialize battery EEPROM data
  initializeBatteryEEPROM();
  
  // Check if button A is held for 10 seconds to enter ref selection mode
  Serial.println("Checking for button A hold to enter ref selection mode...");
  unsigned long buttonHoldStart = millis();
  bool enterRefSelection = false;
  
  while (millis() - buttonHoldStart < 5000) {  // 10 second window
    if (digitalRead(decisionPins[0]) == LOW) {  // Button A pressed
      // Check if held for full 10 seconds
      unsigned long holdStart = millis();
      bool stillHeld = true;
      
      while (millis() - holdStart < 10000 && stillHeld) {  // Must hold for 10 seconds
        if (digitalRead(decisionPins[0]) == HIGH) {  // Button released
          stillHeld = false;
          Serial.println("Button A released before 10 seconds");
          break;
        }
        delay(50);  // Small delay to prevent excessive polling
      }
      
      if (stillHeld) {
        Serial.println("Button A held for 10 seconds - entering ref selection mode");
        enterRefSelection = true;
        break;
      }
    }
    delay(100);  // Check every 100ms
  }
  
  if (enterRefSelection) {
    // Power up display and enter ref selection mode
    setRefNumber();
  } else {
    // Load referee from EEPROM without showing selection screen
    if (EEPROM.read(0) == 255) {
      referee = 1;
    } else {
      referee = EEPROM.read(0);
    }
    Serial.print("Using stored referee number: ");
    Serial.println(referee);
    
    // Power up the display for normal operation
    digitalWrite(DISPLAY_POWER_PIN, LOW);  // Connect to ground
    delay(500);  // Wait for display to stabilize
    
    // Initialize I2C and display
    Wire.begin(SDA_PIN, SCL_PIN);
    u8g2.begin();
    u8g2.clearBuffer();
    u8g2.sendBuffer();
    delay(200);
  }
  
  // EEPROM was already initialized above for restart logic
  // No need to call EEPROM.begin() again in setRefNumber()
  
  // Now that display is powered and initialized, set up the rest
  u8g2.enableUTF8Print();
  u8g2.setContrast(255);
  
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
    updateDisplayWithPriority(WiFi.status() == WL_CONNECTED, false, referee, getBatteryPercentage(), currentDecision, "Server connection lost", reminderMessage);
  }
  mqttClient.loop();
  buttonLoop();
  
  // Check if we need to clear the decision display after downSignal
  if (downSignalReceived && (millis() - downSignalTime >= 3000)) {
    currentDecision = "";
    downSignalReceived = false;
    updateDisplayWithPriority(WiFi.status() == WL_CONNECTED, mqttClient.connected(), referee, getBatteryPercentage(), "", warningMessage, reminderMessage);
  }

  lowBatteryCheck();
  
  // Update display status periodically
  static unsigned long lastDisplayUpdate = 0;
  if (millis() - lastDisplayUpdate >= 1000) {
    updateDisplayWithPriority(WiFi.status() == WL_CONNECTED, mqttClient.connected(), referee, getBatteryPercentage(), currentDecision, warningMessage, reminderMessage);
    lastDisplayUpdate = millis();
  }
}