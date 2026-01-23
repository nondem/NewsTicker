#ifndef NEWSCORE_H
#define NEWSCORE_H

#include <Arduino.h>
#include <vector>
#include <WiFi.h>
#include "Settings.h"

// --- DATA STRUCTURES ---
struct NewsSource {
  String name;
  String url;
  uint16_t color;       
  uint16_t bgColor;     
  uint16_t titleColor;  
};

struct Story {
  String headline;
  String timeStr;
  String url;           // For QR Codes
  time_t timestamp;     
  int sourceIndex;
};

// --- EXTERNAL VARIABLES ---
extern std::vector<Story> megaPool;
extern NewsSource sources[12]; 
extern int failureCount;

// [NEW] Global Status Flag
extern bool lastSyncFailed; 

// --- CORE FUNCTIONS ---
void refreshNewsData(int batchIndex);
int findStrictStory(int startSearchIndex, int forbidSource1, int forbidSource2);

#endif