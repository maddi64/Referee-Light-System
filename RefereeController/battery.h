#ifndef BATTERY_H
#define BATTERY_H

#define BATTERY_PIN_COUNT 4
extern int batteryPins[BATTERY_PIN_COUNT];

extern int batteryPins[];

void batteryMonitoringTask(void *parameter);
void setupBatteryPins();
int getActiveLED(float voltage);
float calculateAverage(float *values, int length);
float getBatteryVoltage();
void updateBatteryLEDs(float voltage, int activeLED, bool flashState);

#endif