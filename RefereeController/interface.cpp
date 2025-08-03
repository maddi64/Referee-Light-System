#include "interface.h"

// Battery empty icon (24x16 pixels)
//static const unsigned char image_battery_empty_bits[] U8X8_PROGMEM = {0x00,0x00,0x00,0xf0,0xff,0x7f,0x08,0x00,0x80,0x08,0x00,0x80,0x0e,0x00,0x80,0x01,0x00,0x80,0x01,0x00,0x80,0x01,0x00,0x80,0x01,0x00,0x80,0x01,0x00,0x80,0x0e,0x00,0x80,0x08,0x00,0x80,0x08,0x00,0x80,0xf0,0xff,0x7f,0x00,0x00,0x00,0x00,0x00,0x00};
static const unsigned char image_battery_empty_bits[] U8X8_PROGMEM = {0x00,0x00,0x00,0xfe,0xff,0x0f,0x01,0x00,0x10,0x01,0x00,0x10,0x01,0x00,0x70,0x01,0x00,0x80,0x01,0x00,0x80,0x01,0x00,0x80,0x01,0x00,0x80,0x01,0x00,0x80,0x01,0x00,0x70,0x01,0x00,0x10,0x01,0x00,0x10,0xfe,0xff,0x0f,0x00,0x00,0x00,0x00,0x00,0x00};
// Cloud sync icon (17x16 pixels)
static const unsigned char image_cloud_sync_bits[] U8X8_PROGMEM = {0x00,0x00,0x00,0xe0,0x03,0x00,0x10,0x04,0x00,0x08,0x08,0x00,0x0c,0x10,0x00,0x02,0x70,0x00,0x01,0x80,0x00,0x41,0x04,0x01,0xe2,0x04,0x01,0xf4,0xf5,0x00,0x40,0x04,0x00,0x40,0x1f,0x00,0x40,0x0e,0x00,0x40,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
// WiFi full icon (19x16 pixels)
static const unsigned char image_wifi_full_bits[] U8X8_PROGMEM = {0x80,0x0f,0x00,0xe0,0x3f,0x00,0x78,0xf0,0x00,0x9c,0xcf,0x01,0xee,0xbf,0x03,0xf7,0x78,0x07,0x3a,0xe7,0x02,0xdc,0xdf,0x01,0xe8,0xb8,0x00,0x70,0x77,0x00,0xa0,0x2f,0x00,0xc0,0x1d,0x00,0x80,0x0a,0x00,0x00,0x07,0x00,0x00,0x02,0x00,0x00,0x00,0x00};



void drawBatteryLevel(int level) {
  // Serial.print("LEVEL RECORDED IS: ");
  // Serial.println(level);
  // Draw battery outline
  u8g2.drawXBMP(104, 1, 24, 16, image_battery_empty_bits);
  
  // Calculate segment width and spacing
  const int segmentWidth = 3;
  const int segmentSpacing = 1;
  const int startX = 123;  // Starting position inside battery outline (right side)
  const int startY = 4;    // Vertical position inside battery outline
  const int segmentHeight = 9;  // Increased height by 1px
  
  // Draw battery segments from right to left
  for(int i = 0; i < 4; i++) {
    if(level >= (4 - i) * 25) {  // Reversed order: 4=100%, 3=75%, 2=50%, 1=25%
      int x = startX - ((i + 1) * (segmentWidth + segmentSpacing));
      u8g2.drawBox(x, startY, segmentWidth, segmentHeight);
    }
  }
}

void drawStatusIcons(bool wifi_status, bool mqtt_status) {
  // Draw WiFi icon
  u8g2.drawXBMP(3, 2, 19, 16, image_wifi_full_bits);
  if (!wifi_status) {
    u8g2.drawLine(3, 2, 22, 18);   // Draw X over icon if disconnected
    u8g2.drawLine(3, 18, 22, 2);
  }
  
  // Draw Cloud Sync icon (MQTT)
  u8g2.drawXBMP(25, 2, 17, 16, image_cloud_sync_bits);
  if (!mqtt_status) {
    u8g2.drawLine(25, 2, 42, 18);  // Draw X over icon if disconnected
    u8g2.drawLine(25, 18, 42, 2);
  }
  
  // Draw Earth icon
  //u8g2.drawXBMP(46, 2, 15, 16, image_earth_bits);
}

void drawWarningMessage(const String& message) {
  if (message.length() > 0) {
    // Create inverse text box at the bottom
    u8g2.drawBox(0, 51, 127, 13);
    
    // Draw text in inverse color
    u8g2.setDrawColor(2);
    u8g2.setFont(u8g2_font_profont10_tr);
    int textWidth = u8g2.getStrWidth(message.c_str());
    int x = (127 - textWidth) / 2;
    u8g2.drawStr(x, 60, message.c_str());
    
    // Reset draw color
    u8g2.setDrawColor(1);
  }
}

void drawMainText(const String& text) {
  if (text == "MAKE DECISION" || text == "DELIBERATION" || text == "SESSION DONE" || text == "MARSHAL BREAK" ) {
    u8g2.setFont(u8g2_font_profont17_tr);
  } else if (text == "TECHNICAL BREAK") {
    u8g2.setFont(u8g2_font_profont15_tr);
  } else {
    u8g2.setFont(u8g2_font_profont22_tr);
  }
  int textWidth = u8g2.getStrWidth(text.c_str());
  int x = (127 - textWidth) / 2;
  u8g2.drawStr(x, 41, text.c_str());
}

void drawRefNumber(int referee) {
  u8g2.setFont(u8g2_font_profont22_tr);
  String refStr = String(referee);
  u8g2.drawStr(46, 17, refStr.c_str());
}

void updateDisplay(bool wifi_status, bool mqtt_status, int referee, int battery_percent, const String& mainText, const String& warning) {
  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);

  // Fix debug output
  // Serial.print("Wifi status: ");
  // Serial.println(wifi_status);
  // Serial.print("MQTT status: ");
  // Serial.println(mqtt_status);
  // Serial.print("Battery: ");
  // Serial.println(battery_percent);
  // Serial.print("Main Text: ");
  // Serial.println(mainText);
  // Serial.print("Warning: ");
  // Serial.println(warning);
  // Serial.println("-------------------");

  // drawStatusIcons(wifi_status, mqtt_status);
  // drawBatteryLevel(battery_percent);
  // if (mainText.length() > 0) {
  //   drawMainText(mainText);
  // }
  // if (warning.length() > 0) {
  //   drawWarningMessage(warning);
  // }

  drawStatusIcons(wifi_status, mqtt_status);
  drawRefNumber(referee);
  drawBatteryLevel(battery_percent);
  if (mainText.length() > 0) {
    drawMainText(mainText);
  }
  if (warning.length() > 0) {
    drawWarningMessage(warning);
  }
  
  u8g2.sendBuffer();
}

void refSelectionDispay(int referee, int timer) {
  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.drawBox(-1, 1, 127, 13);

  u8g2.setDrawColor(2);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(16, 32, "RED BUTTON TO SELECT REF");

  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_profont29_tr);
  String refStr = String(referee);
  u8g2.drawStr(57, 60, refStr.c_str());

  u8g2.setDrawColor(2);
  u8g2.setFont(u8g2_font_profont10_tr);
  u8g2.drawStr(12, 10, "SELECT REFEREE NUMBER");

  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_5x8_tr);
  String timerStr = String(timer);
  u8g2.drawStr(115, 61, timerStr.c_str());

  u8g2.setDrawColor(2);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(12, 25, "WHITE BUTTON TO CHANGE REF");

  u8g2.sendBuffer();
}