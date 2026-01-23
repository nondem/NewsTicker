#include "NewsCore.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_task_wdt.h>
#include <algorithm> 

// --- CONFIGURATION ---
#define MAX_HEADLINE_LEN   110     
// [CRITICAL FIX] 12 Sources * 6 Stories = 72 Total. Fits safely in 80.
#define FETCH_LIMIT_PER_SRC 6      
#define MAX_POOL_SIZE      80      
#define MAX_AGE_SECONDS    172800  

// --- GLOBAL STORAGE ---
std::vector<Story> megaPool;
int failureCount = 0;
bool lastSyncFailed = false; 

// --- SOURCE DEFINITIONS ---
NewsSource sources[12] = {
  { "FOX NEWS",    "https://news.google.com/rss/search?q=site:foxnews.com",   WHITE, DARKRED, YELLOW },
  { "CNN",         "https://news.google.com/rss/search?q=site:cnn.com",       BLACK, WHITE,   RED },
  { "USA TODAY",   "https://news.google.com/rss/search?q=site:usatoday.com",  WHITE, NAVY,    CYAN },
  { "CHRISTIAN SCI", "https://news.google.com/rss/search?q=site:csmonitor.com",    WHITE,  CHARCOAL, YELLOW },

  { "NBC NEWS",      "https://news.google.com/rss/search?q=site:nbcnews.com",       WHITE,  VIOLET,   WHITE },
  { "ABC NEWS",      "https://news.google.com/rss/search?q=site:abcnews.go.com",    WHITE,  BLACK,    WHITE },

  
  { "NY POST",       "https://news.google.com/rss/search?q=site:nypost.com",        WHITE,  RED,      WHITE },

  { "DAILY WIRE",  "https://news.google.com/rss/search?q=site:dailywire.com", WHITE,  BLUE,    WHITE },
 { "NEWSWEEK",      "https://news.google.com/rss/search?q=site:newsweek.com",      WHITE,  RED,      WHITE },
  { "REUTERS",     "https://news.google.com/rss/search?q=site:reuters.com",   ORANGE, CHARCOAL,WHITE },
  { "ASSOC. PRESS","https://news.google.com/rss/search?q=site:apnews.com",    BLACK, GOLD,    BLACK }, 
  { "HUFFPOST",    "https://news.google.com/rss/search?q=site:huffpost.com",  WHITE, TEAL,    WHITE }  
};

String cleanText(String raw) {
  raw.replace("<![CDATA[", ""); raw.replace("]]>", "");
  raw.replace("&apos;", "'"); raw.replace("&#39;", "'");
  raw.replace("&quot;", "\""); raw.replace("&amp;", "&");
  raw.replace("&lt;", "<"); raw.replace("&gt;", ">");
  raw.replace("&nbsp;", " ");
  
  raw.replace("…", "..."); 

  raw.replace("&#8217;", "'"); raw.replace("&#8216;", "'");
  raw.replace("&#8220;", "\""); raw.replace("&#8221;", "\"");
  raw.replace("&#8211;", "-"); raw.replace("&#8212;", "-");
  raw.replace("&#8230;", "...");
  
  raw.replace("’", "'"); raw.replace("“", "\"");
  raw.replace("”", "\""); raw.replace("–", "-");

  raw.replace("<b>", ""); raw.replace("</b>", "");
  raw.replace("<i>", ""); raw.replace("</i>", "");
  raw.replace("<strong>", ""); raw.replace("</strong>", "");

  String upper = raw; upper.toUpperCase();
  if (upper.startsWith("LIVE: ")) raw = raw.substring(6);
  else if (upper.startsWith("WATCH: ")) raw = raw.substring(7);
  else if (upper.startsWith("VIDEO: ")) raw = raw.substring(7);
  else if (upper.startsWith("UPDATE: ")) raw = raw.substring(8);
  else if (upper.startsWith("BREAKING: ")) raw = raw.substring(10);

  String purified = "";
  for (int i = 0; i < raw.length(); i++) {
      char c = raw.charAt(i);
      if (c >= 32 && c <= 126) purified += c;
      else purified += ' ';
      if (i % 20 == 0) esp_task_wdt_reset(); 
  }
  raw = purified;

  raw.replace("\n", " "); raw.replace("\t", " "); raw.replace("\r", " "); 
  while(raw.indexOf("  ") >= 0) {
      raw.replace("  ", " ");
      esp_task_wdt_reset(); 
  }
  raw.trim();

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

bool isValidStory(String headline) {
    if (headline.length() < 20) return false; 
    String upper = headline; upper.toUpperCase();
    
    if (upper.indexOf("TODAYS HEADLINES") >= 0) return false;
    if (upper.indexOf("TODAY'S HEADLINES") >= 0) return false;
    if (upper.indexOf("MORNING BRIEFING") >= 0) return false;
    if (upper.indexOf("EVENING BRIEFING") >= 0) return false;
    if (upper.indexOf("DAILY DIGEST") >= 0) return false;
    if (upper.indexOf("THE LATEST:") >= 0) return false;
    if (upper.indexOf("YOUR MONDAY") >= 0) return false;
    if (upper.indexOf("YOUR TUESDAY") >= 0) return false;
    if (upper.indexOf("YOUR WEDNESDAY") >= 0) return false;
    if (upper.indexOf("YOUR THURSDAY") >= 0) return false;
    if (upper.indexOf("YOUR FRIDAY") >= 0) return false;
    if (upper.indexOf("WEEKEND BRIEFING") >= 0) return false;

    if (upper.indexOf("PODCAST") >= 0) return false;
    if (upper.indexOf("AUDIO:") >= 0) return false;
    if (upper.indexOf("LISTEN:") >= 0) return false;
    if (upper.indexOf("WATCH:") >= 0) return false;

    if (upper.indexOf("SUBSCRIBE TO") >= 0) return false;
    if (upper.indexOf("SIGN UP") >= 0) return false;
    
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

void pruneOldStories() {
    esp_task_wdt_reset(); 
    if (megaPool.empty()) return;
    time_t newest = 0;
    for(const auto& s : megaPool) { 
        if(s.timestamp > newest) newest = s.timestamp; 
    }
    time_t cutoff = newest - MAX_AGE_SECONDS;
    int before = megaPool.size();
    megaPool.erase(
        std::remove_if(megaPool.begin(), megaPool.end(), [cutoff](const Story& s) {
            return (s.timestamp < cutoff);
        }),
        megaPool.end()
    );
    int after = megaPool.size();
    Serial.print("[Prune] Removed "); Serial.print(before - after); 
    Serial.println(" stories older than 48h.");
    esp_task_wdt_reset(); 
}

void fetchAndPool(int sourceIdx) {
  esp_task_wdt_reset();
  
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
        if (ESP.getFreeHeap() < 20000) break; 
        
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

               int suffix = s.headline.lastIndexOf(" - ");
               if (suffix == -1) suffix = s.headline.lastIndexOf(" | ");
               if (suffix > 5) s.headline = s.headline.substring(0, suffix);
               
               if (isValidStory(s.headline)) {
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
      if (failureCount >= 10) ESP.restart(); 
      return; 
  }

  if (ESP.getFreeHeap() < 25000) {
      Serial.println("[NewsCore] Low Heap. Aborting.");
      lastSyncFailed = true; 
      return;
  }

  int start = batchIndex * 6; int end = start + 6;
  megaPool.erase(std::remove_if(megaPool.begin(), megaPool.end(), [start, end](const Story& s) {
        return (s.sourceIndex >= start && s.sourceIndex < end);
    }), megaPool.end());

  for(int i = start; i < end; i++) {
     fetchAndPool(i);
     esp_task_wdt_reset();
  }
  
  pruneOldStories();
  std::random_shuffle(megaPool.begin(), megaPool.end());

  if (megaPool.empty()) {
      Story s;
      s.headline = "SYSTEM: NO NEWS DATA AVAILABLE. WAITING FOR SYNC...";
      s.sourceIndex = 0; s.timeStr = "--:--"; s.url = ""; s.timestamp = 0;
      megaPool.push_back(s);
      s.headline = "CHECKING NETWORK CONNECTION...";
      s.sourceIndex = 1; megaPool.push_back(s);
      lastSyncFailed = true; 
  }

  time_t newest = 0;
  for(const auto& s : megaPool) { if(s.timestamp > newest) newest = s.timestamp; }
  time_t cutoff = newest - 86400; 

  std::stable_sort(megaPool.begin(), megaPool.end(), [cutoff](const Story& a, const Story& b) {
      bool aFresh = (a.timestamp >= cutoff);
      bool bFresh = (b.timestamp >= cutoff);
      if (aFresh && !bFresh) return true;
      if (!aFresh && bFresh) return false;
      return false; 
  });
  
  esp_task_wdt_reset();
  Serial.print("Total Stories: "); Serial.println(megaPool.size());
}

int findStrictStory(int startSearchIndex, int forbidSource1, int forbidSource2) {
  if (megaPool.empty()) return 0;
  int count = 0;
  int idx = startSearchIndex;
  while (count < megaPool.size()) {
     idx = idx % megaPool.size();
     int src = megaPool[idx].sourceIndex;
     if (src != forbidSource1 && src != forbidSource2) return idx;
     idx++; count++;
  }
  return startSearchIndex % megaPool.size();
}