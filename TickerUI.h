#ifndef TICKERUI_H
#define TICKERUI_H

#include <Arduino.h>
#include "NewsCore.h"
#include "DisplayHAL.h"

// --- THEME MANAGEMENT ---
// Rotates the top bar color scheme
void cycleHeaderTheme();
int getCurrentThemeIndex();

// --- DRAWING FUNCTIONS ---
void drawHeader();
void drawWiFiIcon();
void drawRowDirect(int rowIndex, int storyIndex);
// [NEW] Sync Status Indicator
void drawSyncStatus(long remainingMs, bool isSyncing, long intervalMs);
// --- SPECIAL SCREENS ---
void triggerEasterEgg();
void showConfigScreen();

#endif