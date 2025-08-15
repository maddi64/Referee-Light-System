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

void drawOTAWaitingScreen() {
  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  
  // Draw header box
  u8g2.drawBox(0, 0, 128, 16);
  u8g2.setDrawColor(2);
  u8g2.setFont(u8g2_font_profont12_tr);
  String headerText = "UPDATE MODE";
  int headerWidth = u8g2.getStrWidth(headerText.c_str());
  int headerX = (128 - headerWidth) / 2;
  u8g2.drawStr(headerX, 12, headerText.c_str());
  
  // Reset draw color and draw main content
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_profont15_tr);
  String line1 = "Waiting for";
  String line2 = "firmware...";
  int line1Width = u8g2.getStrWidth(line1.c_str());
  int line2Width = u8g2.getStrWidth(line2.c_str());
  int line1X = (128 - line1Width) / 2;
  int line2X = (128 - line2Width) / 2;
  u8g2.drawStr(line1X, 35, line1.c_str());
  u8g2.drawStr(line2X, 50, line2.c_str());
  
  // Draw animated dots
  static int dotCount = 0;
  static unsigned long lastDotUpdate = 0;
  unsigned long currentTime = millis();
  
  if (currentTime - lastDotUpdate > 500) {
    dotCount = (dotCount + 1) % 4;
    lastDotUpdate = currentTime;
  }
  
  u8g2.setFont(u8g2_font_profont12_tr);
  String dots = "";
  for (int i = 0; i < dotCount; i++) {
    dots += ".";
  }
  int dotsWidth = u8g2.getStrWidth(dots.c_str());
  int dotsX = (128 - dotsWidth) / 2 + line2Width / 2;
  u8g2.drawStr(dotsX, 50, dots.c_str());
  
  u8g2.sendBuffer();
}

void drawOTAProgressScreen(int progress, const String& status) {
  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  
  // Draw header box
  u8g2.drawBox(0, 0, 128, 16);
  u8g2.setDrawColor(2);
  u8g2.setFont(u8g2_font_profont12_tr);
  String headerText = "UPDATE MODE";
  int headerWidth = u8g2.getStrWidth(headerText.c_str());
  int headerX = (128 - headerWidth) / 2;
  u8g2.drawStr(headerX, 12, headerText.c_str());
  
  // Reset draw color
  u8g2.setDrawColor(1);
  
  // Draw status text
  u8g2.setFont(u8g2_font_profont11_tr);
  int statusWidth = u8g2.getStrWidth(status.c_str());
  int statusX = (128 - statusWidth) / 2;
  u8g2.drawStr(statusX, 30, status.c_str());
  
  // Draw progress bar background
  u8g2.drawFrame(10, 35, 108, 12);
  
  // Draw progress bar fill
  int fillWidth = (progress * 106) / 100;  // 106 = 108 - 2 (border)
  if (fillWidth > 0) {
    u8g2.drawBox(11, 36, fillWidth, 10);
  }
  
  // Draw percentage text
  u8g2.setFont(u8g2_font_profont15_tr);
  String percentText = String(progress) + "%";
  int percentWidth = u8g2.getStrWidth(percentText.c_str());
  int percentX = (128 - percentWidth) / 2;
  u8g2.drawStr(percentX, 60, percentText.c_str());
  
  u8g2.sendBuffer();
}

void drawOTAReadyScreen(const String& ipAddress) {
  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  
  // Draw header box
  u8g2.drawBox(0, 0, 128, 16);
  u8g2.setDrawColor(2);
  u8g2.setFont(u8g2_font_profont12_tr);
  String headerText = "UPDATE MODE";
  int headerWidth = u8g2.getStrWidth(headerText.c_str());
  int headerX = (128 - headerWidth) / 2;
  u8g2.drawStr(headerX, 12, headerText.c_str());
  
  // Reset draw color
  u8g2.setDrawColor(1);
  
  // Draw "Ready for update" message
  u8g2.setFont(u8g2_font_profont12_tr);
  String readyText = "Ready for update";
  int readyWidth = u8g2.getStrWidth(readyText.c_str());
  int readyX = (128 - readyWidth) / 2;
  u8g2.drawStr(readyX, 30, readyText.c_str());
  
  // Draw "Upload at:" label
  u8g2.setFont(u8g2_font_profont10_tr);
  String uploadText = "Upload at:";
  int uploadWidth = u8g2.getStrWidth(uploadText.c_str());
  int uploadX = (128 - uploadWidth) / 2;
  u8g2.drawStr(uploadX, 42, uploadText.c_str());
  
  // Draw IP address
  u8g2.setFont(u8g2_font_profont12_tr);
  int ipWidth = u8g2.getStrWidth(ipAddress.c_str());
  int ipX = (128 - ipWidth) / 2;
  u8g2.drawStr(ipX, 55, ipAddress.c_str());
  
  u8g2.sendBuffer();
}

void drawOTAUploadingScreen(int progress) {
  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  
  // Draw header box
  u8g2.drawBox(0, 0, 128, 16);
  u8g2.setDrawColor(2);
  u8g2.setFont(u8g2_font_profont12_tr);
  String headerText = "UPDATE MODE";
  int headerWidth = u8g2.getStrWidth(headerText.c_str());
  int headerX = (128 - headerWidth) / 2;
  u8g2.drawStr(headerX, 12, headerText.c_str());
  
  // Reset draw color
  u8g2.setDrawColor(1);
  
  // Draw "Uploading firmware" message
  u8g2.setFont(u8g2_font_profont12_tr);
  String uploadingText = "Uploading firmware";
  int uploadingWidth = u8g2.getStrWidth(uploadingText.c_str());
  int uploadingX = (128 - uploadingWidth) / 2;
  u8g2.drawStr(uploadingX, 30, uploadingText.c_str());
  
  // Draw progress bar background
  u8g2.drawFrame(10, 35, 108, 12);
  
  // Draw progress bar fill
  int fillWidth = (progress * 106) / 100;  // 106 = 108 - 2 (border)
  if (fillWidth > 0) {
    u8g2.drawBox(11, 36, fillWidth, 10);
  }
  
  // Draw percentage text
  u8g2.setFont(u8g2_font_profont15_tr);
  String percentText = String(progress) + "%";
  int percentWidth = u8g2.getStrWidth(percentText.c_str());
  int percentX = (128 - percentWidth) / 2;
  u8g2.drawStr(percentX, 60, percentText.c_str());
  
  u8g2.sendBuffer();
}

void drawOTACompleteScreen(bool success, const String& message) {
  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  
  // Draw header box
  u8g2.drawBox(0, 0, 128, 16);
  u8g2.setDrawColor(2);
  u8g2.setFont(u8g2_font_profont12_tr);
  String headerText = "UPDATE MODE";
  int headerWidth = u8g2.getStrWidth(headerText.c_str());
  int headerX = (128 - headerWidth) / 2;
  u8g2.drawStr(headerX, 12, headerText.c_str());
  
  // Reset draw color
  u8g2.setDrawColor(1);
  
  if (success) {
    // Draw success message
    u8g2.setFont(u8g2_font_profont15_tr);
    String successText = "UPDATE COMPLETE!";
    int successWidth = u8g2.getStrWidth(successText.c_str());
    int successX = (128 - successWidth) / 2;
    u8g2.drawStr(successX, 45, successText.c_str());
  } else {
    // Draw error message
    u8g2.setFont(u8g2_font_profont15_tr);
    String failText = "UPDATE FAILED";
    int failWidth = u8g2.getStrWidth(failText.c_str());
    int failX = (128 - failWidth) / 2;
    u8g2.drawStr(failX, 45, failText.c_str());
  }
  
  // Draw additional message if provided
  if (message.length() > 0) {
    u8g2.setFont(u8g2_font_profont10_tr);
    int msgWidth = u8g2.getStrWidth(message.c_str());
    int msgX = (128 - msgWidth) / 2;
    u8g2.drawStr(msgX, 62, message.c_str());
  }
  
  u8g2.sendBuffer();
}

void drawOTAErrorScreen(const String& error) {
  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  
  // Draw header box
  u8g2.drawBox(0, 0, 128, 16);
  u8g2.setDrawColor(2);
  u8g2.setFont(u8g2_font_profont12_tr);
  String headerText = "UPDATE MODE";
  int headerWidth = u8g2.getStrWidth(headerText.c_str());
  int headerX = (128 - headerWidth) / 2;
  u8g2.drawStr(headerX, 12, headerText.c_str());
  
  // Reset draw color
  u8g2.setDrawColor(1);
  
  // Draw error title
  u8g2.setFont(u8g2_font_profont15_tr);
  String errorTitle = "ERROR";
  int errorTitleWidth = u8g2.getStrWidth(errorTitle.c_str());
  int errorTitleX = (128 - errorTitleWidth) / 2;
  u8g2.drawStr(errorTitleX, 30, errorTitle.c_str());
  
  // Draw error message (split into multiple lines if needed)
  u8g2.setFont(u8g2_font_profont10_tr);
  
  // Simple word wrapping for error message
  String errorMsg = error;
  int maxCharsPerLine = 18;  // Approximate characters that fit per line
  int yPos = 55;
  
  while (errorMsg.length() > 0 && yPos < 64) {
    String line;
    if (errorMsg.length() <= maxCharsPerLine) {
      line = errorMsg;
      errorMsg = "";
    } else {
      // Find last space within character limit
      int spacePos = errorMsg.lastIndexOf(' ', maxCharsPerLine);
      if (spacePos > 0) {
        line = errorMsg.substring(0, spacePos);
        errorMsg = errorMsg.substring(spacePos + 1);
      } else {
        // No space found, just cut at character limit
        line = errorMsg.substring(0, maxCharsPerLine);
        errorMsg = errorMsg.substring(maxCharsPerLine);
      }
    }
    
    int lineWidth = u8g2.getStrWidth(line.c_str());
    int lineX = (128 - lineWidth) / 2;
    u8g2.drawStr(lineX, yPos, line.c_str());
    yPos += 9;  // Line spacing
  }
  
  u8g2.sendBuffer();
}