#ifndef INTERFACE_H
#define INTERFACE_H

#include <Arduino.h>
#include <U8g2lib.h>

// Update display type to match hardware SPI initialization
//extern U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI u8g2;
extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;

void drawBatteryLevel(int level);
void drawStatusIcons(bool wifi_status, bool mqtt_status);
void drawWarningMessage(const String& message);
void drawMainText(const String& text);
void updateDisplay(bool wifi_status, bool mqtt_status, int referee, int battery_percent, const String& mainText = "", const String& warning = "");

void refSelectionDispay(int referee, int timer);

#endif