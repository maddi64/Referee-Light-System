#ifndef BATTERY_H
#define BATTERY_H


extern String warningMessage;
extern bool lowBattery; 
void batteryMonitoringTask(void *parameter);
void setupBatteryPins();
int getBatteryPercentage();
float getBatteryVoltage();
void checkBatteryLevel();

#endif