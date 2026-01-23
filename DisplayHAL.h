#ifndef DISPLAY_HAL_H
#define DISPLAY_HAL_H

#include <Arduino.h>
#include <SPI.h>
#include "Settings.h"

// Initialize SPI and LCD (Call this in setup)
void initDisplay();

// Core Graphics Primitives
void writeCmd(uint8_t cmd);
void writeData(uint8_t data);
void writeData16(uint16_t data);
void setAddrWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
// Signal Meter (Hardcoded Colors)
void drawSignalBars(int x, int y, int rssi, bool force = false);

// Text Rendering (Opaque Mode)
void drawChar(int x, int y, char c, uint16_t color, uint16_t bg, uint8_t size);
void drawText(int x, int y, int w, const char* str, uint16_t color, uint16_t bg, uint8_t size, bool bold = false);

// QR Code Generator
void drawQRCode(const char* url, const char* label);

#endif