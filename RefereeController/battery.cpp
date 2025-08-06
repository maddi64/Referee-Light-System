#include <Arduino.h>
#include <EEPROM.h>
#include "battery.h"
#include "interface.h"

// EEPROM addresses for battery data
#define EEPROM_BASELINE_VOLTAGE_ADDR 2  // Use address 2 (addresses 0,1 used by main code)

// Battery monitoring settings
#define BATTERY_ADC_PIN 35
#define MONITOR_ADC_PIN 39

// Voltage thresholds
#define LOW_BATTERY_THRESHOLD 3.3
#define VOLTAGE_100_CHARGING 4.125
#define CHARGING_VOLTAGE_REDUCTION 0.2

// ADC settings
#define ADC_RESOLUTION 4095.0
#define REFERENCE_VOLTAGE 3.3
#define VOLTAGE_DIVIDER_RATIO 2.0
#define ADC_SAMPLES 10

float baselineVoltage = 4.0;  // Default baseline voltage
float calibrationFactor = 1.03;

// Charging state variables
static float highestChargingVoltage = 0.0;
static bool wasCharging = false;
static bool isFullyCharged = false;

// Charging animation variables
static int currentAnimationBars = 1;
static int baseChargingLevel = 1;
static unsigned long lastAnimationUpdate = 0;
static bool animationGoingUp = true;

#define ANIMATION_INTERVAL 500  // 500ms between bar updates

float readRawVoltage() {
  int32_t adcSum = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    adcSum += analogRead(BATTERY_ADC_PIN);
    delay(5);
  }

  int adcValue = adcSum / ADC_SAMPLES;
  float vadc = (adcValue / ADC_RESOLUTION) * REFERENCE_VOLTAGE;
  float rawVoltage = vadc * VOLTAGE_DIVIDER_RATIO;
  return rawVoltage;
}

void saveBaselineVoltageToEEPROM(float voltage) {
  // Convert float to bytes and save to EEPROM
  byte* voltageBytes = (byte*)&voltage;
  for (int i = 0; i < sizeof(float); i++) {
    EEPROM.write(EEPROM_BASELINE_VOLTAGE_ADDR + i, voltageBytes[i]);
  }
  EEPROM.commit();
}

float loadBaselineVoltageFromEEPROM() {
  float voltage;
  byte* voltageBytes = (byte*)&voltage;
  for (int i = 0; i < sizeof(float); i++) {
    voltageBytes[i] = EEPROM.read(EEPROM_BASELINE_VOLTAGE_ADDR + i);
  }
  
  // Check if EEPROM contains valid data
  if (isnan(voltage) || voltage == 0.0 || voltage > 5.0 || voltage < 2.0) {
    return 4.0;  // Return default if invalid
  }
  return voltage;
}

void initializeBatteryEEPROM() {
  baselineVoltage = loadBaselineVoltageFromEEPROM();
  
  // If loaded value is still invalid, set to current voltage
  if (isnan(baselineVoltage) || baselineVoltage < 2.0 || baselineVoltage > 5.0) {
    float currentVoltage = getBatteryVoltage();
    if (!isnan(currentVoltage) && currentVoltage >= 2.0 && currentVoltage <= 5.0) {
      baselineVoltage = currentVoltage;
      saveBaselineVoltageToEEPROM(baselineVoltage);
    } else {
      baselineVoltage = 4.0;  // Fallback to default
    }
  }
  
  // Serial.print("Loaded baseline voltage from EEPROM: ");
  // Serial.println(baselineVoltage);
}

float getBatteryVoltage() {
  float rawVoltage = readRawVoltage();
  //Serial.println(rawVoltage);
  float calibratedVoltage = rawVoltage * calibrationFactor;
  return calibratedVoltage;
}

int getBatteryBarsFromVoltage(float voltage) {
  if (voltage > 3.9) return 4;      // >75% & <100%
  if (voltage > 3.6) return 3;      // >50% & <=75%
  if (voltage > 3.3) return 2;      // >25% & <=50%
  if (voltage > 3.0) return 1;      // <=25%
  return 0;
}

void resetChargingState(float compensatedVoltage) {
  highestChargingVoltage = compensatedVoltage;
  baseChargingLevel = getBatteryBarsFromVoltage(compensatedVoltage);
  currentAnimationBars = baseChargingLevel;
  animationGoingUp = true;
  isFullyCharged = (compensatedVoltage >= VOLTAGE_100_CHARGING);
  lastAnimationUpdate = millis();
}

void updateChargingAnimation(float compensatedVoltage) {
  unsigned long currentTime = millis();
  
  // Update highest voltage seen during charging (this becomes new baseline)
  if (compensatedVoltage > highestChargingVoltage) {
    highestChargingVoltage = compensatedVoltage;
    int newBaseLevel = getBatteryBarsFromVoltage(compensatedVoltage);
    if (newBaseLevel > baseChargingLevel) {
      baseChargingLevel = newBaseLevel;
      // Reset animation to start from new base level
      currentAnimationBars = baseChargingLevel;
      animationGoingUp = true;
    }
  }
  
  // Check if fully charged
  if (compensatedVoltage >= VOLTAGE_100_CHARGING) {
    isFullyCharged = true;
    return;
  }
  
  // Update animation
  if (currentTime - lastAnimationUpdate >= ANIMATION_INTERVAL) {
    lastAnimationUpdate = currentTime;
    
    if (animationGoingUp) {
      currentAnimationBars++;
      if (currentAnimationBars > 4) {
        // After reaching 4 bars, drop straight back to baseline
        currentAnimationBars = baseChargingLevel;
        // Stay going up to continue the cycle
      }
    }
  }
}

int getBatteryPercentage() {
  float currentVoltage = getBatteryVoltage();
  int adcChargeValue = analogRead(MONITOR_ADC_PIN);
  bool isCharging = (adcChargeValue > 1000);
  
  float voltageToSave = currentVoltage;
  
  if (isCharging) {
    // Reduce voltage by 0.2V when charging before saving
    voltageToSave = currentVoltage - CHARGING_VOLTAGE_REDUCTION;
    
    // When charging starts, reset the baseline to allow increases
    if (!wasCharging) {
      // Just started charging - use current compensated voltage as starting point
      resetChargingState(voltageToSave);
      baselineVoltage = voltageToSave;  // Reset baseline to current compensated voltage
      saveBaselineVoltageToEEPROM(baselineVoltage);
    }
    
    // Update baseline if we see a higher compensated voltage while charging
    if (voltageToSave > baselineVoltage) {
      baselineVoltage = voltageToSave;
      saveBaselineVoltageToEEPROM(baselineVoltage);
      // Serial.print("New higher baseline voltage saved while charging: ");
      // Serial.println(baselineVoltage);
    }
    
    // Handle charging display
    if (isFullyCharged) {
      wasCharging = isCharging;
      return 100;  // Show 4 solid bars when fully charged
    } else {
      updateChargingAnimation(voltageToSave);
      wasCharging = isCharging;
      return currentAnimationBars * 25;  // Convert bars to percentage
    }
  } else {
    // Not charging - only allow baseline to decrease
    if (currentVoltage < baselineVoltage) {
      baselineVoltage = currentVoltage;
      saveBaselineVoltageToEEPROM(baselineVoltage);
      // Serial.print("New lower baseline voltage saved while not charging: ");
      // Serial.println(baselineVoltage);
    }
    // Ignore higher readings when not charging
    
    // Reset charging state when unplugged
    if (wasCharging) {
      isFullyCharged = false;
    }
  }
  
  wasCharging = isCharging;
  
  // Use baseline voltage for battery level calculation when not charging
  int bars = getBatteryBarsFromVoltage(baselineVoltage);
  return bars * 25;  // Convert bars to percentage (25%, 50%, 75%, 100%)
}

void setupBatteryPins() {
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
}

void checkBatteryLevel() {
  float currentVoltage = getBatteryVoltage();
  int adcChargeValue = analogRead(MONITOR_ADC_PIN);
  bool isCharging = (adcChargeValue > 1000);
  
  float checkVoltage;
  if (isCharging) {
    checkVoltage = currentVoltage - CHARGING_VOLTAGE_REDUCTION;
  } else {
    checkVoltage = baselineVoltage;  // Use baseline when not charging
  }
  
  // Debug output
  // Serial.print("Current voltage: ");
  // Serial.println(currentVoltage);
  // Serial.print("Baseline voltage: ");
  // Serial.println(baselineVoltage);
  // Serial.print("Check voltage: ");
  // Serial.println(checkVoltage);
  // Serial.print("Is charging: ");
  // Serial.println(isCharging);
  
  // Low battery warning triggers at 3.3V
  if (!isnan(checkVoltage) && checkVoltage < LOW_BATTERY_THRESHOLD) {
    lowBattery = true;
  } else {
    lowBattery = false;
  }
}

void batteryMonitoringTask(void *parameter) {
  while (true) {
    checkBatteryLevel();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}