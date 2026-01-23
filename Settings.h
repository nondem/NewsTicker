#ifndef SETTINGS_H
#define SETTINGS_H

// --- USER SETTINGS ---
#define EASTER_EGG_TEXT     "Randys Waterfall Ticker 2026"
#define USER_TIMEZONE_HOUR  -5  // EST
#define OFFLINE_MODE        false //broken
// --- SYSTEM SETTINGS ---
#define WDT_TIMEOUT_SECONDS 90  
#define UPDATE_INTERVAL_MS  1200000 // [UPDATED] 20 Minutes (Split Batch Schedule)
#define CAROUSEL_INTERVAL_MS 20000  // 20 Seconds per slide
#define WAVE_DELAY_MS       500          
#define PARSE_TIMEOUT_MS    6000      

// --- PIN DEFINITIONS (CYD / ESP32-2432S028R) ---
#define LCD_CS      15
#define LCD_DC      2
#define LCD_BL      27
#define TOUCH_CS    33
#define TOUCH_IRQ   36
#define SD_CS       5

#define SPI_SCK     14
#define SPI_MISO    12
#define SPI_MOSI    13

// LED Pins (Active LOW)
#define LED_RED     4
#define LED_GREEN   16
#define LED_BLUE    17

// --- COLORS ---
#define BLACK       0x0000
#define WHITE       0xFFFF
#define RED         0xF800
#define DARKRED     0xA000
#define BLUE        0x001F 
#define NAVY        0x000F
#define CYAN        0x07FF
#define YELLOW      0xFFE0
#define GREEN       0x07E0
#define DEEPGREEN   0x0200 
#define CHARCOAL    0x2124 
#define GOLD        0xFEA0
#define ORANGE      0xFD20
#define TEAL        0x0415 
#define DARKBLUE    0x0010
#define VIOLET      0x901F

#endif