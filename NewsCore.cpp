#include "NewsCore.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_task_wdt.h>
#include <algorithm> 

// --- GLOBAL STORAGE ---
std::vector<Story> megaPool;
std::vector<int> playbackQueue; // The "Deck of Cards" for display
int failureCount = 0;
bool lastSyncFailed = false; 

// --- SOURCE DEFINITIONS (18 TOTAL) ---
NewsSource sources[18] = {
  // BATCH A (0-5)
  { "FOX NEWS",    "https://news.google.com/rss/search?q=site:foxnews.com",   WHITE, DARKRED, YELLOW },
  { "CNN",         "https://news.google.com/rss/search?q=site:cnn.com",       BLACK, WHITE,   RED },
  { "USA TODAY",   "https://news.google.com/rss/search?q=site:usatoday.com",  WHITE, NAVY,    CYAN },
  { "CHRISTIAN SCI", "https://news.google.com/rss/search?q=site:csmonitor.com",    WHITE,  CHARCOAL, YELLOW },
  { "NBC NEWS",      "https://news.google.com/rss/search?q=site:nbcnews.com",       WHITE,  VIOLET,   WHITE },
  { "ABC NEWS",      "https://news.google.com/rss/search?q=site:abcnews.go.com",    WHITE,  BLACK,    WHITE },

  // BATCH B (6-11)
  { "NY POST",       "https://news.google.com/rss/search?q=site:nypost.com",        WHITE,  RED,      WHITE },
  { "DAILY WIRE",  "https://news.google.com/rss/search?q=site:dailywire.com", WHITE,  BLUE,    WHITE },
  { "NEWSWEEK",      "https://news.google.com/rss/search?q=site:newsweek.com",      WHITE,  RED,      WHITE },
  { "REUTERS",     "https://news.google.com/rss/search?q=site:reuters.com",   ORANGE, CHARCOAL,WHITE },
  { "ASSOC. PRESS","https://news.google.com/rss/search?q=site:apnews.com",    BLACK, GOLD,    BLACK }, 
  { "HUFFPOST",    "https://news.google.com/rss/search?q=site:huffpost.com",  WHITE, TEAL,    WHITE },

  // BATCH C (12-17)
  { "WSJ",         "https://news.google.com/rss/search?q=site:wsj.com",       BLACK, WHITE,   BLACK },
  { "FORBES",      "https://news.google.com/rss/search?q=site:forbes.com",     WHITE, DARKBLUE, GOLD },
  { "REASON",      "https://news.google.com/rss/search?q=site:reason.com",     BLACK, ORANGE,  BLACK },
  { "SKY NEWS",    "https://news.google.com/rss/search?q=site:news.sky.com",   WHITE, RED,     WHITE },
  { "BBC NEWS",    "https://news.google.com/rss/search?q=site:bbc.com",        WHITE, DARKRED, WHITE },
  { "POLITICO",    "https://news.google.com/rss/search?q=site:politico.com",   WHITE, BLUE,    RED }
};

// --- HELPER: RESET PLAYBACK QUEUE ---
void resetPlaybackQueue() {
    playbackQueue.clear();
    for (int i = 0; i < megaPool.size(); i++) {
        playbackQueue.push_back(i);
    }
    std::random_shuffle(playbackQueue.begin(), playbackQueue.end());
    Serial.print("[NewsCore] Queue Reshuffled. Size: ");
    Serial.println(playbackQueue.size());
}

// --- HELPER: GET NEXT UNIQUE STORY ---
int getNextStoryIndex(const std::vector<int>& forbiddenSources) {
    if (megaPool.empty()) return 0;

    if (playbackQueue.empty()) resetPlaybackQueue();

    std::vector<int> skippedCards;
    int foundIndex = -1;

    // Search deck for non-conflicting story
    while (!playbackQueue.empty()) {
        int idx = playbackQueue.back();
        playbackQueue.pop_back();

        if (idx >= megaPool.size()) continue;

        int src = megaPool[idx].sourceIndex;
        bool conflict = false;
        
        // Check against forbidden list
        for (int banned : forbiddenSources) {
            if (src == banned) { 
                conflict = true; 
                break; 
            }
        }

        if (!conflict) {
            foundIndex = idx;
            break; 
        } else {
            skippedCards.push_back(idx); // Save conflict for later
        }
    }

    // Fallback: If EVERYTHING conflicts, take the first skipped one
    if (foundIndex == -1) {
        if (!skippedCards.empty()) {
            foundIndex = skippedCards[0];
            skippedCards.erase(skippedCards.begin());
        } else {
            // Should not happen unless pool is empty
            resetPlaybackQueue();
            if(!playbackQueue.empty()) {
                foundIndex = playbackQueue.back();
                playbackQueue.pop_back();
            } else {
                return 0;
            }
        }
    }

    // Return skipped cards to the BOTTOM of the deck
    if (!skippedCards.empty()) {
        playbackQueue.insert(playbackQueue.begin(), skippedCards.begin(), skippedCards.end());
    }

    return foundIndex;
}

String cleanText(String raw) {
  // 1. Basic HTML Cleanup
  raw.replace("<![CDATA[", ""); raw.replace("]]>", "");
  raw.replace("&apos;", "'"); raw.replace("&#39;", "'");
  raw.replace("&quot;", "\""); raw.replace("&amp;", "&");
  raw.replace("&lt;", "<"); raw.replace("&gt;", ">");
  raw.replace("&nbsp;", " ");
  raw.replace("…", "..."); 

  // 2. Fancy Quotes/Dashes
  raw.replace("&#8217;", "'"); raw.replace("&#8216;", "'");
  raw.replace("&#8220;", "\""); raw.replace("&#8221;", "\"");
  raw.replace("&#8211;", "-"); raw.replace("&#8212;", "-");
  raw.replace("&#8230;", "...");
  raw.replace("’", "'"); raw.replace("“", "\"");
  raw.replace("”", "\""); raw.replace("–", "-");

  // 3. HTML Tags
  raw.replace("<b>", ""); raw.replace("</b>", "");
  raw.replace("<i>", ""); raw.replace("</i>", "");
  raw.replace("<strong>", ""); raw.replace("</strong>", "");

  // 4. Prefix Scrubbing (Quality Filter)
  String upper = raw; upper.toUpperCase();
  if (upper.startsWith("LIVE: ")) raw = raw.substring(6);
  if (upper.startsWith("WATCH: ")) raw = raw.substring(7);
  if (upper.startsWith("VIDEO: ")) raw = raw.substring(7);
  if (upper.startsWith("UPDATE: ")) raw = raw.substring(8);
  if (upper.startsWith("BREAKING: ")) raw = raw.substring(10);
  if (upper.startsWith("OPINION: ")) raw = raw.substring(9);
  if (upper.startsWith("REVIEW: ")) raw = raw.substring(8);

  // 5. Suffix Scrubbing (Remove " - SourceName")
  int dashSuffix = raw.lastIndexOf(" - ");
  if (dashSuffix > 10) raw = raw.substring(0, dashSuffix);
  
  int pipeSuffix = raw.lastIndexOf(" | ");
  if (pipeSuffix > 10) raw = raw.substring(0, pipeSuffix);

  // 6. Character Purification
  String purified = "";
  for (int i = 0; i < raw.length(); i++) {
      char c = raw.charAt(i);
      if (c >= 32 && c <= 126) purified += c;
      else purified += ' ';
      if (i % 20 == 0) esp_task_wdt_reset(); 
  }
  raw = purified;

  // 7. Whitespace normalization
  raw.replace("\n", " "); raw.replace("\t", " "); raw.replace("\r", " "); 
  while(raw.indexOf("  ") >= 0) {
      raw.replace("  ", " ");
      esp_task_wdt_reset(); 
  }
  raw.trim();

  // 8. Smart Crop (114 Chars)
  if (raw.length() > MAX_HEADLINE_LEN) {
      int cutOff = raw.lastIndexOf(' ', MAX_HEADLINE_LEN - 3);
      if (cutOff > 0) {
          raw = raw.substring(0, cutOff) + "...";
      } else {
          raw = raw.substring(0, MAX_HEADLINE_LEN - 3) + "...";
      }
  }
  return raw;
}

String cleanURL(String raw) {
  raw.replace("\n", ""); raw.replace("\r", ""); 
  raw.replace("\t", ""); raw.replace(" ", "");  
  raw.replace("&amp;", "&"); 
  raw.trim();
  return raw;
}

// Validity Check (Junk Filter)
bool isValidStory(String headline) {
    if (headline.length() < 20) return false; 
    String upper = headline; upper.toUpperCase();
    
    // Hard blocks (Not news)
    if (upper.indexOf("TODAYS HEADLINES") >= 0) return false;
    if (upper.indexOf("MORNING BRIEFING") >= 0) return false;
    if (upper.indexOf("EVENING BRIEFING") >= 0) return false;
    if (upper.indexOf("DAILY DIGEST") >= 0) return false;
    if (upper.indexOf("SUBSCRIBE TO") >= 0) return false;
    if (upper.indexOf("SIGN UP") >= 0) return false;
    if (upper.indexOf("JAVASCRIPT") >= 0) return false;
    if (upper.indexOf("ACCESS DENIED") >= 0) return false;
    if (upper.indexOf("404 NOT FOUND") >= 0) return false;

    // Quality blocks (Clickbait/Fluff)
    if (upper.startsWith("HOW TO ")) return false;
    if (upper.startsWith("BEST OF ")) return false;
    if (upper.startsWith("DEALS: ")) return false;
    if (upper.startsWith("HOROSCOPE")) return false;
    if (upper.startsWith("WORDLE ")) return false;
    if (upper.startsWith("CROSSWORD ")) return false;
    if (upper.startsWith("10 THINGS ")) return false;
    if (upper.startsWith("5 THINGS ")) return false;
    
    return true; 
}

bool safeFind(WiFiClient* stream, const char* target) {
    unsigned long start = millis();
    int len = strlen(target);
    int matchIdx = 0;
    while(millis() - start < PARSE_TIMEOUT_MS) {
        esp_task_wdt_reset(); 
        if(stream->available()) {
            char c = stream->read();
            if(c == target[matchIdx]) {
                matchIdx++;
                if(matchIdx == len) return true; 
            } else { matchIdx = 0; }
        } else { delay(10); }
    }
    return false; 
}

String safeReadUntil(WiFiClient* stream, char terminator) {
    String res = "";
    unsigned long start = millis();
    while(millis() - start < PARSE_TIMEOUT_MS) {
        esp_task_wdt_reset(); 
        if(stream->available()) {
            char c = stream->read();
            if(c == terminator) return res;
            if(res.length() < 300) res += c; 
        } else { delay(10); }
    }
    return res;
}

time_t parseRSSDate(String d) {
  struct tm t = {0};
  int firstSpace = d.indexOf(' ');
  if(firstSpace == -1) return 0;
  String clean = d.substring(firstSpace + 1);
  int s1 = clean.indexOf(' ');
  int s2 = clean.indexOf(' ', s1 + 1);
  int s3 = clean.indexOf(' ', s2 + 1);
  int s4 = clean.indexOf(' ', s3 + 1);
  if(s1 < 0 || s2 < 0 || s3 < 0 || s4 < 0) return 0;
  String m = clean.substring(s1 + 1, s2);
  int mon = 0;
  if(m=="Feb") mon=1; else if(m=="Mar") mon=2; else if(m=="Apr") mon=3;
  else if(m=="May") mon=4; else if(m=="Jun") mon=5; else if(m=="Jul") mon=6;
  else if(m=="Aug") mon=7; else if(m=="Sep") mon=8; else if(m=="Oct") mon=9;
  else if(m=="Nov") mon=10; else if(m=="Dec") mon=11;
  t.tm_mday = clean.substring(0, s1).toInt();
  t.tm_mon = mon;
  t.tm_year = clean.substring(s2 + 1, s3).toInt() - 1900;
  String hms = clean.substring(s3 + 1, s4);
  t.tm_hour = hms.substring(0, 2).toInt();
  t.tm_min = hms.substring(3, 5).toInt();
  t.tm_sec = hms.substring(6, 8).toInt();
  return mktime(&t); 
}

String formatTime(time_t raw) {
  if(raw < 1704067200) return ""; 
  time_t local = raw + (USER_TIMEZONE_HOUR * 3600);
  struct tm* t = gmtime(&local); 
  int hour = t->tm_hour;
  const char* suffix = "AM";
  if (hour >= 12) { suffix = "PM"; if (hour > 12) hour -= 12; }
  if (hour == 0) hour = 12; 
  const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  char buf[30];
  sprintf(buf, "%s %d:%02d %s", days[t->tm_wday], hour, t->tm_min, suffix);
  return String(buf);
}

void ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.reconnect();
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500); esp_task_wdt_reset();
  }
  if (WiFi.status() != WL_CONNECTED) failureCount++; else failureCount = 0;
}

void fetchAndPool(int sourceIdx) {
  esp_task_wdt_reset();
  
  // Heap Guard
  if (ESP.getFreeHeap() < 20000) {
      Serial.println("[NewsCore] Low Heap (<20k). Skipping fetch.");
      return;
  }

  if (megaPool.size() >= MAX_POOL_SIZE) {
      Serial.println("[NewsCore] Max Pool Size Reached. Stopping fetch.");
      return;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(5000); 
  HTTPClient http;
  http.setUserAgent("Mozilla/5.0 (ESP32)");
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  
  if (http.begin(client, sources[sourceIdx].url)) {
    esp_task_wdt_reset(); 
    int httpCode = http.GET();
    esp_task_wdt_reset();
    
    if (httpCode == HTTP_CODE_OK) {
      WiFiClient *stream = http.getStreamPtr();
      int storiesFound = 0;
      
      while(storiesFound < FETCH_LIMIT_PER_SRC) {
        
        if (safeFind(stream, "<item>")) {
           String tempTitle = "", tempDate = "", tempLink = "";
           int attempts = 0;
           while((tempTitle == "" || tempDate == "" || tempLink == "") && attempts < 30) {
               attempts++;
               if (tempTitle == "" && safeFind(stream, "<title>")) {
                   tempTitle = safeReadUntil(stream, '<');
                   tempTitle.replace("<![CDATA[", ""); tempTitle.replace("]]>", "");
               }
               if (tempLink == "" && safeFind(stream, "<link>")) {
                   tempLink = safeReadUntil(stream, '<');
               }
               if (tempDate == "" && safeFind(stream, "<pubDate>")) {
                   tempDate = safeReadUntil(stream, '<');
               }
           }
           if (tempTitle != "") {
               Story s;
               s.headline = cleanText(tempTitle);
               s.url = cleanURL(tempLink);
               
               if (s.url.length() < 12 || !s.url.startsWith("http")) continue;
               if (s.headline.length() < 15) continue; 

               // Local De-Duplication (Same Source Only)
               bool isSameSourceDuplicate = false;
               for(const auto& existing : megaPool) {
                   if (existing.sourceIndex == sourceIdx && existing.headline.equals(s.headline)) {
                       isSameSourceDuplicate = true; break;
                   }
               }

               if (isValidStory(s.headline) && !isSameSourceDuplicate) {
                   s.timestamp = parseRSSDate(tempDate);
                   s.timeStr = formatTime(s.timestamp);
                   s.sourceIndex = sourceIdx;
                   megaPool.push_back(s);
                   storiesFound++;
                   
                   if (megaPool.size() >= MAX_POOL_SIZE) break;
               }
           }
           esp_task_wdt_reset();
        } else { break; }
      }
    } else {
        Serial.print("HTTP Error: "); Serial.println(httpCode);
        lastSyncFailed = true; 
    }
    http.end();
  } else {
      Serial.println("Connection Failed.");
      lastSyncFailed = true;
  }
}

void refreshNewsData(int batchIndex) {
  #ifdef OFFLINE_MODE
  if (OFFLINE_MODE) { return; }
  #endif

  Serial.print("Fetch Start. Batch: "); Serial.println(batchIndex);
  
  if (megaPool.capacity() < 100) megaPool.reserve(100);

  esp_task_wdt_reset();
  lastSyncFailed = false;

  if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[NewsCore] WiFi Down. Attempting Reconnect...");
      ensureWiFi(); 
  }

  if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[NewsCore] WiFi Failure. Aborting.");
      lastSyncFailed = true; 
      failureCount++;
      // Nuclear Option: Reboot after 4 failures
      if (failureCount >= 4) ESP.restart(); 
      return; 
  }

  // 3-PHASE CLEANUP
  int start = batchIndex * 6; 
  int end = start + 6;
  
  megaPool.erase(std::remove_if(megaPool.begin(), megaPool.end(), [start, end](const Story& s) {
        return (s.sourceIndex >= start && s.sourceIndex < end);
    }), megaPool.end());

  // Fetch new data
  for(int i = start; i < end; i++) {
     fetchAndPool(i);
     esp_task_wdt_reset();
  }
  
  // Sort by date (Newest first)
  time_t newest = 0;
  for(const auto& s : megaPool) { if(s.timestamp > newest) newest = s.timestamp; }
  time_t cutoff = newest - MAX_AGE_SECONDS; // 36 Hours

  // Prune very old stories
  megaPool.erase(std::remove_if(megaPool.begin(), megaPool.end(), [cutoff](const Story& s) {
        return (s.timestamp < cutoff);
    }), megaPool.end());

  if (megaPool.empty()) {
      Story s;
      s.headline = "SYSTEM: NO NEWS DATA AVAILABLE. WAITING FOR SYNC...";
      s.sourceIndex = 0; s.timeStr = "--:--"; s.url = ""; s.timestamp = 0;
      megaPool.push_back(s);
      s.headline = "CHECKING NETWORK CONNECTION...";
      s.sourceIndex = 1; megaPool.push_back(s);
      lastSyncFailed = true; 
  }

  // IMPORTANT: Re-build the playback deck because indices have changed
  resetPlaybackQueue();
  
  esp_task_wdt_reset();
  Serial.print("Total Stories: "); Serial.println(megaPool.size());
}