#include <qrcode.h> 
#include <esp_task_wdt.h> 
#include "DisplayHAL.h"

// --- ST7796 COMMANDS ---
#define SWRESET     0x01
#define SLPOUT      0x11
#define DISPON      0x29
#define CASET       0x2A
#define RASET       0x2B
#define RAMWR       0x2C
#define MADCTL      0x36
#define COLMOD      0x3A

// SPI Settings (40MHz)
SPISettings lcdSettings(40000000, MSBFIRST, SPI_MODE0); 

// --- STATIC QR BUFFER ---
static uint8_t qrBuffer[3000]; 

// --- FONT (5x7) ---
const uint8_t font[] = {
  0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00, 0x42, 0x7F, 0x40, 0x00, 0x42, 0x61, 0x51, 0x49, 0x46, 0x21, 0x41, 0x45, 0x4B, 0x31, 
  0x18, 0x14, 0x12, 0x7F, 0x10, 0x27, 0x45, 0x45, 0x45, 0x39, 0x3C, 0x4A, 0x49, 0x49, 0x30, 0x01, 0x71, 0x09, 0x05, 0x03, 
  0x36, 0x49, 0x49, 0x49, 0x36, 0x06, 0x49, 0x49, 0x29, 0x1E, 0x7F, 0x09, 0x09, 0x09, 0x7F, 0x7F, 0x49, 0x49, 0x49, 0x36, 
  0x3E, 0x41, 0x41, 0x41, 0x22, 0x7F, 0x41, 0x41, 0x22, 0x1C, 0x7F, 0x49, 0x49, 0x49, 0x41, 0x7F, 0x09, 0x09, 0x09, 0x01, 
  0x3E, 0x41, 0x49, 0x49, 0x7A, 0x7F, 0x08, 0x08, 0x08, 0x7F, 0x00, 0x41, 0x7F, 0x41, 0x00, 0x20, 0x40, 0x41, 0x3F, 0x01, 
  0x7F, 0x08, 0x14, 0x22, 0x41, 0x7F, 0x40, 0x40, 0x40, 0x40, 0x7F, 0x02, 0x0C, 0x02, 0x7F, 0x7F, 0x04, 0x08, 0x10, 0x7F, 
  0x3E, 0x41, 0x41, 0x41, 0x3E, 0x7F, 0x09, 0x09, 0x09, 0x06, 0x3E, 0x41, 0x51, 0x21, 0x5E, 0x7F, 0x09, 0x19, 0x29, 0x46, 
  0x46, 0x49, 0x49, 0x49, 0x31, 0x01, 0x01, 0x7F, 0x01, 0x01, 0x3F, 0x40, 0x40, 0x40, 0x3F, 0x1F, 0x20, 0x40, 0x20, 0x1F, 
  0x3F, 0x40, 0x38, 0x40, 0x3F, 0x63, 0x14, 0x08, 0x14, 0x63, 0x07, 0x08, 0x70, 0x08, 0x07, 0x61, 0x51, 0x49, 0x45, 0x43  
};

void writeCmd(uint8_t cmd) {
  SPI.beginTransaction(lcdSettings);
  digitalWrite(LCD_DC, LOW);
  digitalWrite(LCD_CS, LOW);
  SPI.transfer(cmd);
  digitalWrite(LCD_CS, HIGH); SPI.endTransaction();
}

void writeData(uint8_t data) {
  SPI.beginTransaction(lcdSettings);
  digitalWrite(LCD_DC, HIGH); digitalWrite(LCD_CS, LOW);
  SPI.transfer(data);
  digitalWrite(LCD_CS, HIGH); SPI.endTransaction();
}

void writeData16(uint16_t data) {
  SPI.beginTransaction(lcdSettings);
  digitalWrite(LCD_DC, HIGH); digitalWrite(LCD_CS, LOW);
  SPI.transfer(data >> 8); SPI.transfer(data & 0xFF);
  digitalWrite(LCD_CS, HIGH); SPI.endTransaction();
}

void setAddrWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  writeCmd(CASET); writeData16(x); writeData16(x + w - 1);
  writeCmd(RASET); writeData16(y); writeData16(y + h - 1);
  writeCmd(RAMWR);
}

void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
  if((x + w) > 480 || (y + h) > 320) return;
  setAddrWindow(x, y, w, h);
  SPI.beginTransaction(lcdSettings);
  digitalWrite(LCD_DC, HIGH); digitalWrite(LCD_CS, LOW);
  for(uint32_t i = 0; i < (uint32_t)w * h; i++) {
    SPI.transfer(color >> 8);
    SPI.transfer(color & 0xFF);
  }
  digitalWrite(LCD_CS, HIGH); SPI.endTransaction();
}

void drawChar(int x, int y, char c, uint16_t color, uint16_t bg, uint8_t size) {
  // [FIX] Manual Case Conversion (Safer than toupper)
  if (c >= 'a' && c <= 'z') c -= 32;

  uint8_t charBuffer[5] = {0,0,0,0,0};
  const uint8_t* ptr = charBuffer;
  int idx = -1;
  
  if (c >= '0' && c <= '9') idx = (c - '0') * 5;
  else if (c >= 'A' && c <= 'Z') idx = 50 + (c - 'A') * 5;
  
  if (idx != -1) {
      ptr = &font[idx];
  } else {
    // Symbol Mapping
    if (c == '-') { charBuffer[0]=0x08; charBuffer[1]=0x08; charBuffer[2]=0x08; charBuffer[3]=0x08; charBuffer[4]=0x08; }
    else if (c == '.') { charBuffer[2]=0x40; } 
    else if (c == ':') { charBuffer[2]=0x22; } 
    else if (c == '(') { charBuffer[1]=0x1C; charBuffer[2]=0x22; }
    else if (c == ')') { charBuffer[0]=0x22; charBuffer[1]=0x1C; }
    else if (c == '\'') { charBuffer[1]=0x02; } 
    else if (c == '"')  { charBuffer[0]=0x06; charBuffer[2]=0x06; } 
    else if (c == '?')  { charBuffer[0]=0x20; charBuffer[1]=0x40; charBuffer[2]=0x45; charBuffer[3]=0x48; charBuffer[4]=0x30; } 
    else {
        // [CRITICAL FIX] Unknown Char -> Empty Space (was Block)
        // This hides the "Ghost Square" if a bad char sneaks in
        charBuffer[0]=0x00; charBuffer[1]=0x00; charBuffer[2]=0x00; charBuffer[3]=0x00; charBuffer[4]=0x00; 
    }
  }
  
  for (int i = 0; i < 5; i++) {
    uint8_t line = ptr[i];
    for (int j = 0; j < 8; j++) {
      if (line & 0x1) {
        if (size == 1) fillRect(x + i, y + j, 1, 1, color);
        else fillRect(x + i * size, y + j * size, size, size, color);
      } else {
        if (size == 1) fillRect(x + i, y + j, 1, 1, bg);
        else fillRect(x + i * size, y + j * size, size, size, bg);
      }
      line >>= 1;
    }
  }
}

void drawText(int x, int y, int w, const char* str, uint16_t color, uint16_t bg, uint8_t size, bool bold) {
  int curX = x;
  int curY = y;
  int charWidth = 6 * size; 
  int lineHeight = 8 * size;
  const char* p = str;
  
  while (*p) {
    char c = *p;
    if (c == '\n') {
      curX = x; curY += lineHeight + 4; p++; continue;
    }
    if (curX + charWidth > x + w) {
      curX = x; curY += lineHeight + 4;
    }
    fillRect(curX, curY, charWidth, lineHeight, bg);
    if (c != ' ') {
      drawChar(curX, curY, c, color, bg, size);
      if (bold) drawChar(curX + 1, curY, c, color, bg, size);
    }
    curX += charWidth;
    p++;
  }
}

void drawQRCode(const char* url, const char* label) {
    esp_task_wdt_reset();

    if (url == NULL || strlen(url) < 10) {
         fillRect(0, 0, 480, 320, WHITE);
         drawText(10, 150, 460, "ERROR: LINK INVALID", RED, WHITE, 2);
         drawText(10, 180, 460, "NO URL FOUND", RED, WHITE, 2);
         drawText(10, 285, 460, "TAP TO EXIT", BLACK, WHITE, 2);
         return;
    }

    int version = 15; 
    
    if (strlen(url) > 1200) {
         fillRect(0, 0, 480, 320, WHITE);
         drawText(10, 150, 460, "ERROR: URL TOO LONG", RED, WHITE, 2);
         drawText(10, 285, 460, "TAP TO EXIT", BLACK, WHITE, 2);
         return;
    }

    QRCode qrcode;
    qrcode_initText(&qrcode, qrBuffer, version, 0, url);
    
    fillRect(0, 0, 480, 320, WHITE);
    
    int scale = 3; 
    int size = qrcode.size * scale;
    int startX = (480 - size) / 2;
    int startY = 44; 
    
    for (uint8_t y = 0; y < qrcode.size; y++) {
        esp_task_wdt_reset(); 
        yield();
        for (uint8_t x = 0; x < qrcode.size; x++) {
            if (qrcode_getModule(&qrcode, x, y)) {
                fillRect(startX + (x * scale), startY + (y * scale), scale, scale, BLACK);
            }
        }
    }

    drawText(10, 10, 460, "LONG PRESS TO EXIT", BLACK, WHITE, 2);
    if (label != NULL) drawText(10, 285, 460, label, BLACK, WHITE, 2);
}

void drawSignalBars(int x, int y, int rssi, bool force) {
  int activeBars = 0;
  if (rssi > -50) activeBars = 5;       
  else if (rssi > -60) activeBars = 4;  
  else if (rssi > -70) activeBars = 3;  
  else if (rssi > -80) activeBars = 2; 
  else if (rssi > -90) activeBars = 1;  
  
  static int lastBars = -1;
  if (!force && activeBars == lastBars) return;
  lastBars = activeBars;

  if (force) {
      fillRect(x, 0, 80, 20, BLACK); 
      drawText(x + 8, 6, 30, "WiFi", WHITE, BLACK, 1);
  } else {
      fillRect(x + 36, 0, 44, 20, BLACK);
  }

  int startX = x + 36; 
  for (int i = 0; i < 5; i++) {
    int h = 4 + (i * 3);         
    int xOffset = startX + (i * 6);   
    int yOffset = 19 - h;             
    uint16_t color = CHARCOAL; 
    if (i < activeBars) {
       if (i == 0) color = RED;               
       else if (i < 2) color = ORANGE;        
       else color = GREEN;                    
    }
    fillRect(xOffset, yOffset, 4, h, color);
  }
}

void initDisplay() {
  pinMode(LCD_CS, OUTPUT); pinMode(LCD_DC, OUTPUT); pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_CS, HIGH); digitalWrite(LCD_BL, HIGH);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, LCD_CS);
  writeCmd(SWRESET); delay(120);
  writeCmd(SLPOUT);  delay(120);
  writeCmd(COLMOD); writeData(0x55);
  writeCmd(MADCTL); writeData(0x28); 
  writeCmd(DISPON); delay(20);
}