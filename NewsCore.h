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
  String url;           
  time_t timestamp;     
  int sourceIndex;
};

// --- EXTERNAL VARIABLES ---
extern std::vector<Story> megaPool;
extern NewsSource sources[18]; 
extern int failureCount;
extern bool lastSyncFailed; 

// --- CORE FUNCTIONS ---
void refreshNewsData(int batchIndex);

// Returns the index of the next unique story, avoiding sources in the forbidden list
int getNextStoryIndex(const std::vector<int>& forbiddenSources); 

// Call this if the pool changes drastically to force a reshuffle
void resetPlaybackQueue();

#endif