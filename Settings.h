#ifndef SETTINGS_H
#define SETTINGS_H

// --- USER SETTINGS ---
#define EASTER_EGG_TEXT     "Randys Waterfall Ticker 2026"
#define USER_TIMEZONE_HOUR  -5  // EST
#define OFFLINE_MODE        false 

// --- SYSTEM SETTINGS ---
#define WDT_TIMEOUT_SECONDS 90  
#define UPDATE_INTERVAL_MS  900000  // 15 Minutes (Cycles 1/3rd of sources each time)
#define CAROUSEL_INTERVAL_MS 15000  // 15 Seconds per slide
#define WAVE_DELAY_MS       500          
#define PARSE_TIMEOUT_MS    10000   // [UPDATED] 10 Seconds (Stall Killer)
#define SOURCE_FETCH_TIMEOUT_MS 20000  // Max time per source fetch
#define ITEM_PARSE_TIMEOUT_MS   8000   // Max time per item parse

// Limits based on user request
#define MAX_POOL_SIZE       180     // Accommodates 30 sources
#define MAX_HEADLINE_LEN    114     // Hard crop for display width
#define FETCH_LIMIT_PER_SRC 6       // 30 * 6 = 180 max stories
#define MAX_AGE_SECONDS     129600  // 36 Hours

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
#define GREY        0x8410
#define MAROON      0x8000
#define PURPLE      0x8010
#define DARKGREEN   0x02A0

#endif