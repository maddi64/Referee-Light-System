#include <Arduino.h>
#include "battery.h"

// Battery monitoring settings
#define BATTERY_ADC_PIN 35
#define MONITOR_ADC_PIN 39

// PWM settings
#define PWM_MAX_DUTY 100
#define PWM_OFF 0

// Non-charging voltage thresholds
#define VOLTAGE_25 3.2
#define VOLTAGE_50 3.5
#define VOLTAGE_75 3.8
#define VOLTAGE_100 4.0

// Charging voltage threshold
#define VOLTAGE_100_CHARGING 4.125

#define FLASH_INTERVAL 500
#define NUM_READINGS 10
#define ADC_SAMPLES 10  // Number of ADC samples to average for a single reading

// Critical battery voltage threshold
#define CRITICAL_BATTERY_VOLTAGE 3.1

// Calibration constants
#define DEFAULT_CALIBRATION_FACTOR 1.08  // Initial calibration factor (4.1/3.7)
float calibrationFactor = DEFAULT_CALIBRATION_FACTOR;
#define ADC_RESOLUTION 4095.0
#define REFERENCE_VOLTAGE 3.3
#define VOLTAGE_DIVIDER_RATIO 2.0

// Variable to track lowest recorded voltage
float lowestVoltage = VOLTAGE_100;

// Reads a single voltage value with averaging to reduce noise
float readRawVoltage() {
  int32_t adcSum = 0;
  
  // Take multiple samples and average them
  for (int i = 0; i < ADC_SAMPLES; i++) {
    adcSum += analogRead(BATTERY_ADC_PIN);
    delay(50); // Small delay between readings
  }
  
  int adcValue = adcSum / ADC_SAMPLES;
  
  // Convert ADC value to voltage
  float rawVoltage = (adcValue / ADC_RESOLUTION) * REFERENCE_VOLTAGE * VOLTAGE_DIVIDER_RATIO;
  
  return rawVoltage;
}

// Gets the calibrated battery voltage
float getBatteryVoltage() {
  float rawVoltage = readRawVoltage();
  float calibratedVoltage = rawVoltage * calibrationFactor;
  
  return calibratedVoltage;
}

// Calculates the average of an array of values
float calculateAverage(float *values, int length) {
  float sum = 0;
  int validReadings = 0;
  
  for (int i = 0; i < length; i++) {
    if (values[i] > 0) {  // Only count valid readings
      sum += values[i];
      validReadings++;
    }
  }
  
  // Avoid division by zero
  return (validReadings > 0) ? (sum / validReadings) : 0;
}

// Sets up battery indicator pins
void setupBatteryPins() {
  for (int j = 0; j < BATTERY_PIN_COUNT; j++) {
    pinMode(batteryPins[j], OUTPUT);
    analogWrite(batteryPins[j], PWM_OFF);
  }
  
  // Set ADC resolution and attenuation
  analogReadResolution(12); // Set ADC resolution to 12 bits
  analogSetAttenuation(ADC_11db); // Set attenuation for full voltage range
  
  // Initialize serial for debugging if not already initialized
  if (!Serial) {
    Serial.begin(115200);
    delay(1000); // Give time for serial to initialize
  }
  
  Serial.println("Battery monitoring initialized with PWM control");
  Serial.print("Initial calibration factor: ");
  Serial.println(calibrationFactor);
}

// Determines which LED should be active based on voltage and charging state
int getActiveLED(float voltage, bool isCharging) {
  float fullVoltage = isCharging ? VOLTAGE_100_CHARGING : VOLTAGE_100;
  
  if (voltage >= fullVoltage) return batteryPins[3];
  if (voltage >= VOLTAGE_75) return batteryPins[2];
  if (voltage >= VOLTAGE_50) return batteryPins[1];
  if (voltage >= VOLTAGE_25) return batteryPins[0];
  return -1; // No LED if voltage is below all thresholds
}

// Updates the battery LED display
void updateBatteryLEDs(float voltage, int activeLED, bool flashState) {
  int adcChargeValue = analogRead(MONITOR_ADC_PIN);
  bool isCharging = (adcChargeValue > 1000);
  float fullVoltage = isCharging ? VOLTAGE_100_CHARGING : VOLTAGE_100;
  bool isFullyCharged = (voltage >= fullVoltage);
  bool isCriticallyLow = (!isCharging && voltage < CRITICAL_BATTERY_VOLTAGE);
  
  // Update lowest voltage tracking
  if (!isCharging) {
    if (voltage < lowestVoltage) {
      lowestVoltage = voltage;
    }
    // Use lowestVoltage instead of actual voltage for display
    voltage = lowestVoltage;
  } else {
    // Reset lowest voltage when charging
    lowestVoltage = VOLTAGE_100;
  }
  
  // Reset all LEDs first
  for (int i = 0; i < BATTERY_PIN_COUNT; i++) {
    analogWrite(batteryPins[i], PWM_OFF);
  }
  
  if (isCharging) {
    // If fully charged, show all LEDs at full brightness
    if (isFullyCharged) {
      for (int i = 0; i < BATTERY_PIN_COUNT; i++) {
        analogWrite(batteryPins[i], PWM_MAX_DUTY);
      }
    } else {
      // In charging mode but not full, display LEDs based on battery level
      bool ledStates[BATTERY_PIN_COUNT];
      ledStates[0] = (voltage >= VOLTAGE_25);
      ledStates[1] = (voltage >= VOLTAGE_50);
      ledStates[2] = (voltage >= VOLTAGE_75);
      ledStates[3] = (voltage >= fullVoltage);
      
      // Display all LEDs that should be on, except the active LED
      for (int i = 0; i < BATTERY_PIN_COUNT; i++) {
        if (batteryPins[i] != activeLED) {
          analogWrite(batteryPins[i], ledStates[i] ? PWM_MAX_DUTY : PWM_OFF);
        }
      }
      
      // Only flash the next LED to indicate charging
      if (activeLED != -1) {
        analogWrite(activeLED, flashState ? PWM_MAX_DUTY : PWM_OFF);
      }
    }
  } else {
    // In normal non-charging mode
    
    // If critically low battery, flash the first LED regardless of other conditions
    if (isCriticallyLow) {
      // Flash only the first LED
      analogWrite(batteryPins[0], flashState ? PWM_MAX_DUTY : PWM_OFF);
      
      // Turn off all other LEDs
      for (int i = 1; i < BATTERY_PIN_COUNT; i++) {
        analogWrite(batteryPins[i], PWM_OFF);
      }
    } else {
      // Normal battery display - show appropriate LEDs based on voltage level
      for (int i = 0; i < BATTERY_PIN_COUNT; i++) {
        bool shouldBeOn = false;
        
        switch (i) {
          case 0: shouldBeOn = (voltage >= VOLTAGE_25); break;
          case 1: shouldBeOn = (voltage >= VOLTAGE_50); break;
          case 2: shouldBeOn = (voltage >= VOLTAGE_75); break;
          case 3: shouldBeOn = (voltage >= fullVoltage); break;
        }
        
        analogWrite(batteryPins[i], shouldBeOn ? PWM_MAX_DUTY : PWM_OFF);
      }
    }
  }
}

// Main battery monitoring task
void batteryMonitoringTask(void *parameter) {
  bool flashState = LOW;
  unsigned long lastFlashTime = 0;
  unsigned long lastDebugTime = 0;
  const int debugInterval = 2000; // Debug print interval in ms
  
  float voltageReadings[NUM_READINGS] = {0};
  int currentReadingIndex = 0;
  
  // Print header for debug output
  Serial.println("Battery Monitoring Started");
  Serial.println("Raw V\tCal V\tAvg V\tLowest V");
  
  while (true) {
    // Get raw and calibrated voltage
    float rawVoltage = readRawVoltage();
    float calibratedVoltage = rawVoltage * calibrationFactor;
    
    // Store the calibrated voltage in the circular buffer
    voltageReadings[currentReadingIndex] = calibratedVoltage;
    currentReadingIndex = (currentReadingIndex + 1) % NUM_READINGS;
    
    // Calculate average voltage
    float averageVoltage = calculateAverage(voltageReadings, NUM_READINGS);
    
    // Check charging state
    bool isCharging = (analogRead(MONITOR_ADC_PIN) > 1000);
    
    // Determine which LED should flash based on voltage and charging state
    int activeLED = getActiveLED(averageVoltage, isCharging);
    
    // Update LEDs
    updateBatteryLEDs(averageVoltage, activeLED, flashState);
    
    // Handle flashing logic for the active LED
    unsigned long currentTime = millis();
    if (currentTime - lastFlashTime >= FLASH_INTERVAL) {
      lastFlashTime = currentTime;
      flashState = !flashState; // Toggle the flashing state
    }
    
    // Print debug info periodically
    if (currentTime - lastDebugTime >= debugInterval) {
      lastDebugTime = currentTime;
      
      Serial.print(rawVoltage, 2);
      Serial.print("V\t");
      Serial.print(calibratedVoltage, 2);
      Serial.print("V\t");
      Serial.print(averageVoltage, 2);
      Serial.print("V\t");
      Serial.print(lowestVoltage, 2);
      Serial.println("V");
    }
    
    // Delay to allow other tasks to run
    vTaskDelay(pdMS_TO_TICKS(100)); // 100 ms delay
  }
}

// Function to update calibration factor (can be called from main code)
void calibrateVoltage() {
  Serial.println("Starting calibration procedure");
  Serial.println("Please measure actual battery voltage with a multimeter");
  Serial.println("and enter the value in the Serial Monitor.");
  
  // Print current readings
  float rawVoltage = readRawVoltage();
  float currentCalibrated = rawVoltage * calibrationFactor;
  
  Serial.print("Current raw reading: ");
  Serial.print(rawVoltage);
  Serial.println("V");
  
  Serial.print("Current calibrated reading: ");
  Serial.print(currentCalibrated);
  Serial.println("V");
  
  Serial.println("Enter actual measured voltage:");
  
  // Wait for serial input
  while (!Serial.available()) {
    delay(100);
  }
  
  // Read actual voltage from serial
  float actualVoltage = Serial.parseFloat();
  
  if (actualVoltage > 0 && rawVoltage > 0) {
    // Calculate new calibration factor
    calibrationFactor = actualVoltage / rawVoltage;
    
    Serial.print("New calibration factor: ");
    Serial.println(calibrationFactor, 4);
    Serial.println("Calibration complete!");
  } else {
    Serial.println("Invalid values, calibration aborted.");
  }
}