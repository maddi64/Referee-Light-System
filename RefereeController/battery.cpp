#include <Arduino.h>
#include "battery.h"

// Battery monitoring settings
#define BATTERY_ADC_PIN 35
#define MONITOR_ADC_PIN 39

// Voltage thresholds
#define VOLTAGE_25 3.2
#define VOLTAGE_50 3.5
#define VOLTAGE_75 3.8
#define VOLTAGE_100 4.0
#define VOLTAGE_100_CHARGING 4.125
#define CRITICAL_BATTERY_VOLTAGE 3.1

// ADC settings
#define ADC_RESOLUTION 4095.0
#define REFERENCE_VOLTAGE 3.3
#define VOLTAGE_DIVIDER_RATIO 2.0
#define NUM_READINGS 10
#define ADC_SAMPLES 10

float lowestVoltage = VOLTAGE_100;
float calibrationFactor = 1.1;


float readRawVoltage() {
  int32_t adcSum = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    adcSum += analogRead(BATTERY_ADC_PIN);
    delay(5);
  }

  int adcValue = adcSum / ADC_SAMPLES;
  // Serial.print("ADC VALUE: ");
  // Serial.println(adcValue);

  float vadc = (adcValue / ADC_RESOLUTION) * REFERENCE_VOLTAGE;
  // Serial.print("VADC: ");
  // Serial.println(vadc);

  float rawVoltage = vadc * VOLTAGE_DIVIDER_RATIO;
  // Serial.print("RAW VOLTAGE IS: ");
  // Serial.println(rawVoltage);

  return rawVoltage;
}

float getBatteryVoltage() {
  float rawVoltage = readRawVoltage();
  float calibratedVoltage = rawVoltage * calibrationFactor;
  // Serial.print("VOLTAGE IS: ");
  // Serial.println(calibratedVoltage);
  return calibratedVoltage;
}

int getBatteryPercentage() {
  float voltage = getBatteryVoltage();
  int adcChargeValue = analogRead(MONITOR_ADC_PIN);
  bool isCharging = (adcChargeValue > 1000);
  
  if (!isCharging) {
    if (voltage < lowestVoltage) {
      lowestVoltage = voltage;
    }
    voltage = lowestVoltage;
  } else {
    lowestVoltage = VOLTAGE_100;
    lowBattery = false;
  }
  
  if (voltage >= VOLTAGE_100) return 100;
  if (voltage >= VOLTAGE_75) return 75;
  if (voltage >= VOLTAGE_50) return 50;
  if (voltage >= VOLTAGE_25) return 25;
  return 0;
}

void setupBatteryPins() {
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
}

// float globalVoltage() {
//   return currentVoltage;
// }

void checkBatteryLevel(float voltage) {
  if (voltage < 3.1) {
    lowBattery = true; 
  } else {
    lowBattery = false;
  }
}


void batteryMonitoringTask(void *parameter) {
  float voltageReadings[NUM_READINGS] = {0};
  int currentReadingIndex = 0;
  
  while (true) {
    float voltage = getBatteryVoltage();
    checkBatteryLevel(voltage);
    voltageReadings[currentReadingIndex] = voltage;
    currentReadingIndex = (currentReadingIndex + 1) % NUM_READINGS;
    
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}