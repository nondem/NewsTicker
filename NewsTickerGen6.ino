/*
 * RANDY'S NEWS TICKER v50 (FINAL PRODUCTION BUILD)
 * - 18 Sources (3-Batch Cycle)
 * - Smart 24h Cleanse (Replaces a download slot)
 * - Generous 10s Timeout
 */
#include <QRCode_Library.h> 
#include <WiFi.h>
#include <esp_task_wdt.h> 
#include <ArduinoOTA.h>
#include <qrcode.h> 
#include "RandyNet.h"
#include "Settings.h"
#include "DisplayHAL.h"
#include "NewsCore.h"
#include "TickerUI.h"

RandyNet myWifi("Randy-News-Config");
bool waveActive = false;
int waveStep = 0;
unsigned long lastWaveStepTime = 0;
unsigned long waveStartTime = 0; 

int activeRowIndices[3] = {0, 0, 0};

bool qrMode = false;
int qrSelection = 0;
int batchState = 0;
bool isFastBoot = true;

// Easter Egg State
int touchCounter = 0;
unsigned long lastTapTime = 0;

void updateNews() {
  refreshNewsData(batchState);
  
  // Cycle 0 -> 1 -> 2 -> 0
  batchState++;
  if (batchState > 2) batchState = 0;
  
  // Force reset queue to ensure we see new data
  resetPlaybackQueue();

  // Draw immediately with new safe logic
  std::vector<int> usedSources;
  
  // 1. Get first story
  activeRowIndices[0] = getNextStoryIndex(usedSources);
  if(activeRowIndices[0] < megaPool.size()) usedSources.push_back(megaPool[activeRowIndices[0]].sourceIndex);
  
  // 2. Get second story (avoiding source of #1)
  activeRowIndices[1] = getNextStoryIndex(usedSources);
  if(activeRowIndices[1] < megaPool.size()) usedSources.push_back(megaPool[activeRowIndices[1]].sourceIndex);
  
  // 3. Get third story (avoiding source of #1 and #2)
  activeRowIndices[2] = getNextStoryIndex(usedSources);
  
  drawHeader();
  drawRowDirect(0, activeRowIndices[0]);
  drawRowDirect(1, activeRowIndices[1]);
  drawRowDirect(2, activeRowIndices[2]);
}

void enterQRMode() {
    qrMode = true;
    qrSelection = 0; 
    drawQRForSelection();
}

void drawQRForSelection() {
    int storyIdx = activeRowIndices[qrSelection];
    if (storyIdx >= megaPool.size()) return;
    Story s = megaPool[storyIdx];
    drawQRCode(s.url.c_str(), s.headline.c_str());
}

void exitQRMode() {
    qrMode = false;
    drawHeader();
    drawRowDirect(0, activeRowIndices[0]);
    drawRowDirect(1, activeRowIndices[1]);
    drawRowDirect(2, activeRowIndices[2]);
}

void setup() {
  delay(1000);
  Serial.begin(115200);
  pinMode(TOUCH_CS, OUTPUT); pinMode(SD_CS, OUTPUT);
  pinMode(TOUCH_IRQ, INPUT);
  pinMode(LED_RED, OUTPUT); pinMode(LED_GREEN, OUTPUT); pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, HIGH); digitalWrite(LED_BLUE, HIGH);
  digitalWrite(TOUCH_CS, HIGH);
  digitalWrite(SD_CS, HIGH); 
  delay(500); 
  
  initDisplay();

  fillRect(0, 0, 480, 320, BLACK);
  drawText(10, 150, 460, "BOOTING SYSTEM...", WHITE, BLACK, 2);

  myWifi.autoConnect(180, showConfigScreen);
  WiFi.setSleep(false);
  fillRect(0, 0, 480, 320, BLACK);
  drawText(10, 150, 460, "WIFI: CONNECTED!", GREEN, BLACK, 2);
  delay(1000);

  ArduinoOTA.setHostname("RandyTicker");
  ArduinoOTA.onStart([]() { digitalWrite(LCD_BL, LOW); esp_task_wdt_reset(); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) { esp_task_wdt_reset(); });
  ArduinoOTA.onEnd([]() { digitalWrite(LCD_BL, HIGH); });
  ArduinoOTA.begin();

  randomSeed(micros());

  esp_task_wdt_deinit();
  esp_task_wdt_config_t wdt_config = { .timeout_ms = WDT_TIMEOUT_SECONDS * 1000, .trigger_panic = true };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);
}

void loop() {
  static unsigned long lastFetch = 0;
  static unsigned long lastCarousel = millis();
  static unsigned long lastSecond = 0;
  
  ArduinoOTA.handle();
  esp_task_wdt_reset();
  
  long remaining = UPDATE_INTERVAL_MS - (millis() - lastFetch);
  if (lastFetch == 0 || remaining < 0) remaining = 0;

  // --- [NEW] SMART REBOOT (24H CLEANSE) ---
  // If uptime > 24 hours AND we are < 60 seconds from the next download...
  // Reboot instead of downloading. This gives a fresh RAM slate instantly.
  if (millis() > 86400000UL && remaining < 60000) {
      Serial.println("[System] 24H Cleanse Triggered. Rebooting...");
      drawText(10, 0, 400, "DAILY MAINTENANCE...", YELLOW, BLACK, 2, true);
      delay(2000);
      ESP.restart();
  }

  if (!qrMode) {
      drawSyncStatus(remaining, false);
      if (millis() - lastSecond >= 2000) {
          drawWiFiIcon();
          lastSecond = millis();
      }
  }

  // --- ANIMATION SAFETY RESET ---
  if (waveActive && millis() - waveStartTime > 3000) {
      Serial.println("[UI] Animation stuck. Resetting.");
      waveActive = false;
  }

  // --- CAROUSEL TRIGGER ---
  if (!qrMode && !waveActive && millis() - lastCarousel > CAROUSEL_INTERVAL_MS) {
     if(megaPool.size() > 0) {
        waveActive = true;
        waveStep = 0;
        waveStartTime = millis(); 
        cycleHeaderTheme(); 
        drawHeader();

        // Use 'usedSources' to track and prevent duplicate sources on screen
        std::vector<int> usedSources;
        
        // 1. Get first story
        int next0 = getNextStoryIndex(usedSources);
        if (next0 < megaPool.size()) usedSources.push_back(megaPool[next0].sourceIndex);
        
        // 2. Get second story (avoiding source of #1)
        int next1 = getNextStoryIndex(usedSources);
        if (next1 < megaPool.size()) usedSources.push_back(megaPool[next1].sourceIndex);
        
        // 3. Get third story (avoiding source of #1 and #2)
        int next2 = getNextStoryIndex(usedSources);
        
        activeRowIndices[0] = next0; activeRowIndices[1] = next1; activeRowIndices[2] = next2;
        
        lastCarousel = millis();
        lastWaveStepTime = millis();
     } else {
         lastCarousel = millis();
     }
  }

  // --- WAVE STEPPER ---
  if (waveActive && !qrMode) {
      if (waveStep == 0) {
          drawRowDirect(0, activeRowIndices[0]);
          waveStep++; lastWaveStepTime = millis();
      }
      else if (waveStep == 1 && millis() - lastWaveStepTime > WAVE_DELAY_MS) {
          drawRowDirect(1, activeRowIndices[1]);
          waveStep++; lastWaveStepTime = millis();
      }
      else if (waveStep == 2 && millis() - lastWaveStepTime > WAVE_DELAY_MS) {
          drawRowDirect(2, activeRowIndices[2]);
          waveActive = false; 
      }
  }

  // --- FETCH TRIGGER ---
  if (remaining == 0 && !qrMode) { 
    if (WiFi.status() == WL_CONNECTED) {
        drawSyncStatus(0, true);
        updateNews();
        
        if (isFastBoot) {
            // First boot: Wait only 3 mins for next batch (to fill pool faster)
            lastFetch = millis() - (UPDATE_INTERVAL_MS - 180000);
            isFastBoot = false;
        } else {
            lastFetch = millis();
        }
    } 
    else {
        Serial.println("WiFi Down! Retrying in 60s...");
        WiFi.reconnect();
        lastFetch = millis() - (UPDATE_INTERVAL_MS - 60000); 
    }
    if (lastFetch == 0) lastFetch = 1;
  }

  // --- INPUT HANDLING ---
  if (digitalRead(TOUCH_IRQ) == LOW) { 
    unsigned long startPress = millis();
    bool isLongPress = false;
    
    while(digitalRead(TOUCH_IRQ) == LOW) {
       esp_task_wdt_reset();
       delay(10);
       if (millis() - startPress > 800) { 
           isLongPress = true;
           while(digitalRead(TOUCH_IRQ) == LOW) { esp_task_wdt_reset(); delay(10); }
           break;
       }
    }
    
    if (isLongPress) {
        if (!qrMode) {
            drawSyncStatus(0, true);
            updateNews();
            lastFetch = millis();
        } else {
            exitQRMode();
        }
    } else {
        if (millis() - lastTapTime < 600) {
            touchCounter++;
        } else {
            touchCounter = 1;
        }
        lastTapTime = millis();
        if (touchCounter >= 5) {
             triggerEasterEgg();
             touchCounter = 0;
             if (qrMode) exitQRMode(); 
        } 
        else {
             if (!qrMode) enterQRMode();
             else {
                 qrSelection = (qrSelection + 1) % 3;
                 drawQRForSelection();
             }
        }
    }
    delay(200);
  }
  delay(50);
}