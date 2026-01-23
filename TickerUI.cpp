#include "TickerUI.h"
#include <esp_task_wdt.h>

struct HeaderTheme { uint16_t text; uint16_t bg; };
HeaderTheme themes[] = {
  {YELLOW, DEEPGREEN}, {WHITE, NAVY}, {GOLD, DARKRED}, {CYAN, CHARCOAL}, {WHITE, DARKBLUE}
};
int currentThemeIdx = 0;

void cycleHeaderTheme() { currentThemeIdx = (currentThemeIdx + 1) % 5; }
int getCurrentThemeIndex() { return currentThemeIdx; }

void drawHeader() {
  fillRect(0, 0, 480, 20, BLACK);
  // Separator is now covered by the sync bar area usually, but we keep it
  fillRect(0, 19, 480, 1, CHARCOAL);
  
  if (lastSyncFailed) {
      // [FIX] Moved UP to 0 to prevent touching the bar
      drawText(10, 0, 200, "SYNC ERROR", RED, BLACK, 2, false);
  } else {
      // [FIX] Moved UP to 0
      drawText(10, 0, 200, "NEWS TICKER", WHITE, BLACK, 2, false);
  }
  
  int rssi = WiFi.RSSI();
  if (WiFi.status() != WL_CONNECTED) rssi = -999;
  
  // [FIX] WiFi meter moved to Y=0
  drawSignalBars(400, 0, rssi, true); 
}

void drawWiFiIcon() {
  int rssi = WiFi.RSSI();
  if (WiFi.status() != WL_CONNECTED) rssi = -999;
  drawSignalBars(400, 0, rssi, false); 
}

void drawSyncStatus(long remainingMs, bool isSyncing) {
    int barWidth = 480;
    if (!isSyncing && remainingMs > 0) {
        barWidth = map(remainingMs, 0, UPDATE_INTERVAL_MS, 0, 480);
    }
    
    // Bar is at Y=18 (height 2). Text at Y=0 (height 16). Gap is Y=16,17. Perfect.
    if (isSyncing) {
        fillRect(0, 18, 480, 2, BLUE); 
    } else {
        fillRect(0, 18, barWidth, 2, BLUE); 
        if (barWidth < 480) fillRect(barWidth, 18, 480 - barWidth, 2, CHARCOAL);
    }

    static bool lastStateWasSync = false;
    static bool firstRun = true;
    static bool lastStateWasError = false;
    
    bool statusChanged = (isSyncing != lastStateWasSync) || (lastSyncFailed != lastStateWasError);

    if (statusChanged || firstRun) {
        // Clear Title Area (Height 18 to leave bar alone)
        fillRect(0, 0, 400, 18, BLACK); 

        if (isSyncing) {
             drawText(10, 0, 380, "UPDATING...", CYAN, BLACK, 2, false);
        } 
        else if (lastSyncFailed) {
             drawText(10, 0, 380, "SYNC ERROR", RED, BLACK, 2, false);
        }
        else {
             drawText(10, 0, 200, "NEWS TICKER", WHITE, BLACK, 2, false);
        }
        
        lastStateWasSync = isSyncing;
        lastStateWasError = lastSyncFailed;
        firstRun = false;
    }
}

void drawRowDirect(int rowIndex, int storyIndex) {
  if (storyIndex >= megaPool.size()) return;
  Story s = megaPool[storyIndex];
  NewsSource src = sources[s.sourceIndex];
  int yPos = 20 + (rowIndex * 100);
  
  fillRect(0, yPos, 480, 100, src.bgColor);
  drawText(10, yPos + 8, 460, src.name.c_str(), src.titleColor, src.bgColor, 2, true);
  fillRect(0, yPos + 28, 480, 2, src.color);
  if (s.timeStr != "") drawText(300, yPos + 8, 170, s.timeStr.c_str(), src.color, src.bgColor, 2, false);
  drawText(10, yPos + 35, 460, s.headline.c_str(), src.color, src.bgColor, 2, false);
}

void triggerEasterEgg() {
    fillRect(0, 0, 480, 320, BLACK);
    drawText(50, 150, 400, EASTER_EGG_TEXT, GREEN, BLACK, 2, true);
    long start = millis();
    while(millis() - start < 5000) {
        esp_task_wdt_reset(); 
        delay(100);
    }
}

void showConfigScreen() {
  fillRect(0, 0, 480, 320, BLACK);
  drawText(10, 100, 460, "STATUS: WIFI FAILED.", RED, BLACK, 2, true);
  drawText(10, 160, 460, "CONNECT TO THIS WIFI:", WHITE, BLACK, 2, false);
  drawText(10, 190, 460, "Randys-News-Config", YELLOW, BLACK, 2, false);
  drawText(10, 240, 460, "THEN BROWSE TO IP:", WHITE, BLACK, 2, false);
  drawText(10, 270, 460, "http://1.1.1.1", YELLOW, BLACK, 2, false);
}