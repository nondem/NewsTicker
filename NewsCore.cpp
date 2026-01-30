#include "NewsCore.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_task_wdt.h>
#include <algorithm>
#include <set> 

// --- GLOBAL STORAGE ---
std::vector<Story> megaPool;
std::vector<int> playbackQueue; // The "Deck of Cards" for display
int failureCount = 0;
bool lastSyncFailed = false; 

// --- SOURCE DEFINITIONS (30 TOTAL) ---
NewsSource sources[30] = {
    // BATCH A (0-5) - PROBLEMATIC SOURCES FOR DEBUGGING
    { "VALDOSTA DAILY",  "https://news.google.com/rss/search?q=site:valdostadailytimes.com",    BLACK, GOLD,    BLACK,  false },
    { "THOMASVILLE T-E", "https://news.google.com/rss/search?q=site:timesenterprise.com",      WHITE, RED,     BLACK,  false },
    { "MOULTRIE OBS",    "https://news.google.com/rss/search?q=site:moultrieobserver.com",      WHITE, MAROON,  WHITE,  false },
    { "TALLY REPORTS",   "https://tallahasseereports.com/feed/",                                WHITE, MAROON,  WHITE,  true  },
    { "BAINBRIDGE POST", "https://thepostsearchlight.com/feed/",                                WHITE, PURPLE,  GOLD,   true  },
    { "WAKULLA SUN",     "https://thewakullasun.com/feed/",                                     WHITE, RED,     WHITE,  true  },

    // BATCH B (6-11)
    { "GREENE PUB",      "https://www.greenepublishing.com/feed/",                              WHITE, DARKGREEN, WHITE, true  },
    { "APALACH TIMES",   "https://news.google.com/rss/search?q=site:apalachicolatimes.com",     WHITE, BLUE,    CYAN,   false },
    { "SUWANNEE DEM",    "https://news.google.com/rss/search?q=site:suwanneedemocrat.com",      WHITE, BLUE,    WHITE,  false },
    { "HAVANA HERALD",   "https://theherald.online/feed/",                                      BLACK, WHITE,   BLACK,  true  },
    { "WJHG NEWS 7",     "https://news.google.com/rss/search?q=site:wjhg.com",                  WHITE, RED,     BLUE,   false },
    { "CNN",             "https://news.google.com/rss/search?q=site:cnn.com",                   BLACK, WHITE,   RED,    false },

    // BATCH C (12-17)
    { "USA TODAY",       "https://news.google.com/rss/search?q=site:usatoday.com",              WHITE, NAVY,    CYAN,   false },
    { "NBC NEWS",        "https://news.google.com/rss/search?q=site:nbcnews.com",               WHITE, VIOLET,  WHITE,  false },
    { "ABC NEWS",        "https://news.google.com/rss/search?q=site:abcnews.go.com",            WHITE, BLACK,   WHITE,  false },
    { "NY POST",         "https://news.google.com/rss/search?q=site:nypost.com",                WHITE, RED,     WHITE,  false },
    { "CHRISTIAN SCI",   "https://news.google.com/rss/search?q=site:csmonitor.com",             WHITE, CHARCOAL, YELLOW, false },
    { "DAILY WIRE",      "https://news.google.com/rss/search?q=site:dailywire.com",             WHITE, BLUE,    WHITE,  false },

    // BATCH D (18-23)
    { "NEWSWEEK",        "https://news.google.com/rss/search?q=site:newsweek.com",              WHITE, RED,     WHITE,  false },
    { "REUTERS",         "https://news.google.com/rss/search?q=site:reuters.com",               ORANGE, CHARCOAL, WHITE, false },
    { "ASSOC. PRESS",    "https://news.google.com/rss/search?q=site:apnews.com",                BLACK, GOLD,    BLACK,  false },
    { "FLA POLITICS",    "https://floridapolitics.com/feed/",                                   WHITE, ORANGE,  NAVY,   true  },
    { "HUFFPOST",        "https://news.google.com/rss/search?q=site:huffpost.com",              WHITE, TEAL,    WHITE,  false },
    { "FOX NEWS",        "https://news.google.com/rss/search?q=site:foxnews.com",               WHITE, DARKRED, YELLOW, false },

    // BATCH E (24-29)
    { "WSJ",             "https://news.google.com/rss/search?q=site:wsj.com",                   BLACK, WHITE,   BLACK,  false },
    { "FORBES",          "https://news.google.com/rss/search?q=site:forbes.com",                WHITE, DARKBLUE, GOLD,  false },
    { "REASON",          "https://news.google.com/rss/search?q=site:reason.com",                BLACK, ORANGE,  BLACK,  false },
    { "SKY NEWS",        "https://news.google.com/rss/search?q=site:news.sky.com",              WHITE, RED,     WHITE,  false },
    { "BBC NEWS",        "https://news.google.com/rss/search?q=site:bbc.com",                   WHITE, DARKRED, WHITE,  false },
    { "POLITICO",        "https://news.google.com/rss/search?q=site:politico.com",              WHITE, BLUE,    RED,    false }
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

String stripWpMediaTags(String raw) {
    int idx = raw.indexOf("<img");
    while (idx >= 0) {
            int end = raw.indexOf('>', idx);
            if (end < 0) break;
            raw.remove(idx, end - idx + 1);
            idx = raw.indexOf("<img");
    }

    idx = raw.indexOf("<figure");
    while (idx >= 0) {
            int end = raw.indexOf("</figure>", idx);
            if (end >= 0) {
                    int close = raw.indexOf('>', end);
                    if (close < 0) close = end + 9;
                    raw.remove(idx, close - idx + 1);
            } else {
                    int close = raw.indexOf('>', idx);
                    if (close < 0) break;
                    raw.remove(idx, close - idx + 1);
            }
            idx = raw.indexOf("<figure");
    }
    return raw;
}

String stripAllHtmlTags(String raw) {
    String out = "";
    bool inTag = false;
    for (int i = 0; i < raw.length(); i++) {
            char c = raw.charAt(i);
            if (c == '<') { inTag = true; continue; }
            if (c == '>') { inTag = false; continue; }
            if (!inTag) out += c;
            if (i % 64 == 0) esp_task_wdt_reset();
    }
    return out;
}

bool isReadMoreOnly(String raw) {
    String test = raw;
    test.trim();
    String upper = test; upper.toUpperCase();
    if (upper == "") return true;
    if (upper.indexOf("READ MORE") >= 0 && test.length() < 80) return true;
    if (upper.indexOf("CONTINUE READING") >= 0 && test.length() < 80) return true;
    return false;
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
    int consecutiveTimeouts = 0;
    const int MAX_TIMEOUTS = 3;
    
    while(millis() - start < PARSE_TIMEOUT_MS) {
        esp_task_wdt_reset(); 
        if(stream->available()) {
            consecutiveTimeouts = 0;
            char c = stream->read();
            if(c == target[matchIdx]) {
                matchIdx++;
                if(matchIdx == len) return true;
            } else { 
                matchIdx = (c == target[0]) ? 1 : 0;
            }
        } else { 
            consecutiveTimeouts++;
            if (consecutiveTimeouts >= MAX_TIMEOUTS) {
                Serial.println("[DEBUG] Stream timeout in safeFind");
                return false;
            }
            delay(10); 
        }
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

String safeReadUntilEndTagWithTimeout(WiFiClient* stream, const char* endTag, int maxLen, int timeoutMs) {
    String res = "";
    String tail = "";
    int endLen = strlen(endTag);
    unsigned long start = millis();
    int consecutiveTimeouts = 0;
    bool hitMaxLen = false;
    
    while (millis() - start < (unsigned long)timeoutMs) {
        esp_task_wdt_reset();
        if (stream->available()) {
            consecutiveTimeouts = 0;
            char c = stream->read();
            if (res.length() < maxLen) res += c;
            else hitMaxLen = true;
            tail += c;
            if (tail.length() > endLen) tail.remove(0, tail.length() - endLen);
            if (tail.endsWith(endTag)) {
                if (res.endsWith(endTag)) res.remove(res.length() - endLen);
                if (hitMaxLen) Serial.print("[WARN] Item truncated at "), Serial.print(maxLen), Serial.println(" bytes");
                return res;
            }
        } else { 
            consecutiveTimeouts++;
            if (consecutiveTimeouts > 5) {
                Serial.println("[DEBUG] Stream timeout in safeReadUntilTag");
                break;
            }
            delay(10); 
        }
    }
    if (!res.endsWith(endTag)) {
        Serial.println("[WARN] Item incomplete (no closing tag found)");
        return "";
    }
    return res;
}

String safeReadUntilEndTag(WiFiClient* stream, const char* endTag, int maxLen) {
    String res = "";
    String tail = "";
    int endLen = strlen(endTag);
    unsigned long start = millis();
    while (millis() - start < PARSE_TIMEOUT_MS) {
        esp_task_wdt_reset();
        if (stream->available()) {
            char c = stream->read();
            if (res.length() < maxLen) res += c;
            tail += c;
            if (tail.length() > endLen) tail.remove(0, tail.length() - endLen);
            if (tail.endsWith(endTag)) {
                if (res.endsWith(endTag)) res.remove(res.length() - endLen);
                return res;
            }
        } else { delay(10); }
    }
    return res;
}

String extractTagValue(const String& xml, const char* openTag, const char* closeTag) {
    int start = xml.indexOf(openTag);
    if (start < 0) return "";
    start += strlen(openTag);
    int end = xml.indexOf(closeTag, start);
    if (end < 0) return "";
    return xml.substring(start, end);
}

time_t parseRSSDate(String d) {
  struct tm t = {0};
  d.trim();
  if (d.length() < 20) {
      Serial.print("[WARN] Date too short: "), Serial.println(d);
      return 0;
  }
  int firstSpace = d.indexOf(' ');
  if(firstSpace == -1) return 0;
  String clean = d.substring(firstSpace + 1);
  int s1 = clean.indexOf(' ');
  int s2 = clean.indexOf(' ', s1 + 1);
  int s3 = clean.indexOf(' ', s2 + 1);
  int s4 = clean.indexOf(' ', s3 + 1);
  if(s1 < 0 || s2 < 0 || s3 < 0 || s4 < 0) return 0;
  String m = clean.substring(s1 + 1, s2);
  int mon = -1;
  if(m=="Jan") mon=0; else if(m=="Feb") mon=1; else if(m=="Mar") mon=2; else if(m=="Apr") mon=3;
  else if(m=="May") mon=4; else if(m=="Jun") mon=5; else if(m=="Jul") mon=6;
  else if(m=="Aug") mon=7; else if(m=="Sep") mon=8; else if(m=="Oct") mon=9;
  else if(m=="Nov") mon=10; else if(m=="Dec") mon=11;
  if (mon < 0) {
      Serial.print("[WARN] Invalid month: "), Serial.println(m);
      return 0;
  }
  int day = clean.substring(0, s1).toInt();
  int year = clean.substring(s2 + 1, s3).toInt();
  String hms = clean.substring(s3 + 1, s4);
  int hour = hms.substring(0, 2).toInt();
  int minute = hms.substring(3, 5).toInt();
  int second = hms.substring(6, 8).toInt();
  if (day < 1 || day > 31) { Serial.println("[WARN] Day out of range"); return 0; }
  if (hour < 0 || hour > 23) { Serial.println("[WARN] Hour out of range"); return 0; }
  if (minute < 0 || minute > 59) { Serial.println("[WARN] Minute out of range"); return 0; }
  if (second < 0 || second > 59) { Serial.println("[WARN] Second out of range"); return 0; }
  if (year < 2020 || year > 2100) { Serial.println("[WARN] Year out of range"); return 0; }
  t.tm_mday = day;
  t.tm_mon = mon;
  t.tm_year = year - 1900;
  t.tm_hour = hour;
  t.tm_min = minute;
  t.tm_sec = second;
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
    Serial.print("[NewsCore] Fetching: ");
    Serial.println(sources[sourceIdx].name);
  
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
    
    Serial.print("[DEBUG] HTTP Code: "); Serial.println(httpCode);
    
    if (httpCode == HTTP_CODE_OK) {
      WiFiClient *stream = http.getStreamPtr();
      if (!stream) {
          Serial.println("[ERROR] Failed to get stream pointer");
          lastSyncFailed = true;
          http.end();
          return;
      }
      int storiesFound = 0;
      int itemsProcessed = 0;
      int consecutiveParseFailures = 0;
      unsigned long sourceStart = millis();
      
      // Pre-build dedup set for this source (O(n) once, not O(n²) per item)
      std::set<String> existingHeadlines;
      for(const auto& s : megaPool) {
          if (s.sourceIndex == sourceIdx) {
              existingHeadlines.insert(s.headline);
          }
      }
      
      while(storiesFound < FETCH_LIMIT_PER_SRC && (millis() - sourceStart) < SOURCE_FETCH_TIMEOUT_MS) {
        
        if (safeFind(stream, "<item>")) {
           itemsProcessed++;
           String tempTitle = "", tempDate = "", tempLink = "", tempDesc = "", tempContent = "";
           bool isWp = sources[sourceIdx].isWordpress;
           // WordPress sources have large content blocks; increase buffer for full extraction
           int itemMaxLen = isWp ? 3000 : 1500;
           String itemXml = safeReadUntilEndTagWithTimeout(stream, "</item>", itemMaxLen, ITEM_PARSE_TIMEOUT_MS);
           if (itemXml == "") {
               Serial.print("[DEBUG] Item #"); Serial.print(itemsProcessed); Serial.println(" - Parse timeout");
               continue;
           }
           
           Serial.print("[DEBUG] Item #"); Serial.print(itemsProcessed); Serial.print(" XML len: "); Serial.println(itemXml.length());

           tempTitle = extractTagValue(itemXml, "<title>", "</title>");
           tempLink = extractTagValue(itemXml, "<link>", "</link>");
           tempDate = extractTagValue(itemXml, "<pubDate>", "</pubDate>");

           Serial.print("[DEBUG]   Title: "); Serial.println(tempTitle.length() > 50 ? tempTitle.substring(0, 50) + "..." : tempTitle);
           Serial.print("[DEBUG]   Link: "); Serial.println(tempLink.length() > 50 ? tempLink.substring(0, 50) + "..." : tempLink);
           Serial.print("[DEBUG]   Date: "); Serial.println(tempDate);

           if (isWp) {
               tempDesc = extractTagValue(itemXml, "<description>", "</description>");
               tempContent = extractTagValue(itemXml, "<content:encoded>", "</content:encoded>");
               if (tempContent.length() > 200) tempContent = tempContent.substring(0, 200);
           }

           tempTitle.replace("<![CDATA[", ""); tempTitle.replace("]]>", "");
           tempLink.replace("<![CDATA[", ""); tempLink.replace("]]>", "");
           tempDate.replace("<![CDATA[", ""); tempDate.replace("]]>", "");
           if (tempDesc != "") {
               tempDesc.replace("<![CDATA[", ""); tempDesc.replace("]]>", "");
               tempDesc = stripWpMediaTags(tempDesc);
               tempDesc = stripAllHtmlTags(tempDesc);
               tempDesc = cleanText(tempDesc);
           }
           if (tempContent != "") {
               tempContent.replace("<![CDATA[", ""); tempContent.replace("]]>", "");
               tempContent = stripWpMediaTags(tempContent);
               tempContent = stripAllHtmlTags(tempContent);
               tempContent = cleanText(tempContent);
           }

           if (tempDesc == "" || isReadMoreOnly(tempDesc)) {
               if (tempContent != "") tempDesc = tempContent;
           }

           if (tempTitle == "" && tempDesc != "") {
               tempTitle = tempDesc;
           }

           if (tempTitle != "") {
               Story s;
               s.headline = cleanText(tempTitle);
               s.url = cleanURL(tempLink);
               
               Serial.print("[DEBUG]   Cleaned Title: "); Serial.println(s.headline.length() > 50 ? s.headline.substring(0, 50) + "..." : s.headline);
               Serial.print("[DEBUG]   Cleaned URL: "); Serial.println(s.url);
               
               if (s.url.length() < 12 || !s.url.startsWith("http")) {
                   Serial.println("[DEBUG]   REJECTED: Invalid URL");
                   continue;
               }
               if (s.headline.length() < 15) {
                   Serial.println("[DEBUG]   REJECTED: Headline too short");
                   continue;
               }

               // Local De-Duplication (Same Source Only) - O(1) set lookup
               if (existingHeadlines.find(s.headline) != existingHeadlines.end()) {
                   Serial.println("[DEBUG]   REJECTED: Duplicate");
                   continue;
               }

               if (!isValidStory(s.headline)) {
                   Serial.println("[DEBUG]   REJECTED: Failed validation filter");
                   continue;
               }

               s.timestamp = parseRSSDate(tempDate);
               if (s.timestamp == 0) {
                   Serial.println("[DEBUG]   REJECTED: Invalid date");
                   consecutiveParseFailures++;
                   continue;
               }
               consecutiveParseFailures = 0;
               
               s.timeStr = formatTime(s.timestamp);
               s.sourceIndex = sourceIdx;
               megaPool.push_back(s);
               existingHeadlines.insert(s.headline);
               storiesFound++;
               Serial.print("[DEBUG]   ACCEPTED! Stories from this source: "); Serial.println(storiesFound);
               
               if (megaPool.size() >= MAX_POOL_SIZE) break;
           } else {
               Serial.println("[DEBUG]   REJECTED: No title");
               consecutiveParseFailures++;
           }
           
           if (consecutiveParseFailures > 3) {
               Serial.println("[DEBUG] Too many parse failures. Aborting source.");
               break;
           }
           
           if (ESP.getFreeHeap() < 15000) {
               Serial.println("[DEBUG] Low heap during fetch. Aborting source.");
               break;
           }
           
           esp_task_wdt_reset();
        } else { break; }
      }
      
      Serial.print("[NewsCore] Source Complete. Items processed: "); Serial.print(itemsProcessed);
      Serial.print(", Stories added: "); Serial.println(storiesFound);
      
      if ((millis() - sourceStart) >= SOURCE_FETCH_TIMEOUT_MS) {
                    Serial.println("[NewsCore] Source fetch timeout.");
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
  
  // Exempt problematic and local news sources from age pruning
  // These sources return older aggregated articles and should not be filtered by age
  // BATCH A: 0=Valdosta, 1=Thomasville, 2=Moultrie (Google News)
  // BATCH B: 5=Wakulla Sun (older articles in feed)
  const int exemptSources[] = {0, 1, 2, 5};
  const int exemptSourceCount = 4;
  
  bool isExempt[30] = {false};
  for(int i = 0; i < exemptSourceCount; i++) {
      isExempt[exemptSources[i]] = true;
  }
  
  Serial.print("[DEBUG] Before age pruning: "); Serial.println(megaPool.size());

  // Prune very old stories, but NEVER prune exempt sources
  megaPool.erase(std::remove_if(megaPool.begin(), megaPool.end(), [cutoff, &isExempt](const Story& s) {
        return (s.timestamp < cutoff && !isExempt[s.sourceIndex]);
    }), megaPool.end());
    
  Serial.print("[DEBUG] After age pruning: "); Serial.println(megaPool.size());
  
  Serial.println("[DEBUG] Stories in pool by source:");
  for(int src = 0; src < 30; src++) {
      int count = 0;
      for(const auto& s : megaPool) { if (s.sourceIndex == src) count++; }
      if (count > 0) {
          Serial.print("[DEBUG]   Source "); Serial.print(src); Serial.print(" (");
          Serial.print(sources[src].name); Serial.print("): "); Serial.println(count);
      }
  }

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
  
  // DEBUG: Log all stories in pool by source
  Serial.println("[DEBUG] Stories in pool by source:");
  for(int src = 0; src < 30; src++) {
      int count = 0;
      for(const auto& s : megaPool) {
          if (s.sourceIndex == src) count++;
      }
      if (count > 0) {
          Serial.print("[DEBUG]   Source "); Serial.print(src); Serial.print(" ("); 
          Serial.print(sources[src].name); Serial.print("): "); Serial.println(count);
      }
  }
}