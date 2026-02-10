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

// --- SOURCE-LEVEL STATISTICS ---
struct SourceStats {
  int fetched = 0;      // Total items fetched
  int accepted = 0;    // Items added to pool
  int duplicates = 0;  // Duplicate rejections
  int parseErrors = 0; // Parse/validation failures
  int consecutiveFails = 0; // Consecutive failures
  unsigned long lastFetchMs = 0; // Last fetch timestamp
};
SourceStats sourceStats[30] = {};


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
    { "WFSU NEWS",       "https://news.wfsu.org/wfsu-local-news/rss.xml",                       WHITE, NAVY,      GOLD,   true },
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
    #ifdef DEBUG_MODE
    if (DEBUG_MODE) {
        Serial.println("[DEBUG] Queue composition by source:");
        int sourceCounts[30] = {0};
        for (int idx : playbackQueue) {
            if (idx < megaPool.size()) {
                sourceCounts[megaPool[idx].sourceIndex]++;
            }
        }
        for (int i = 0; i < 30; i++) {
            if (sourceCounts[i] > 0) {
                Serial.print("[DEBUG]   Source "); Serial.print(i); 
                Serial.print(" ("); Serial.print(sources[i].name); 
                Serial.print("): "); Serial.println(sourceCounts[i]);
            }
        }
    }
    #endif
}

// --- HELPER: GET NEXT UNIQUE STORY ---
int getNextStoryIndex(const std::vector<int>& forbiddenSources) {
    #ifdef DEBUG_MODE
    if (DEBUG_MODE) {
        Serial.print("[DEBUG] getNextStoryIndex called. Forbidden sources: ");
        for (int src : forbiddenSources) {
            Serial.print(src); Serial.print(",");
        }
        Serial.print(" | Queue size: "); Serial.println(playbackQueue.size());
    }
    #endif
    
    if (megaPool.empty()) {
        #ifdef DEBUG_MODE
        if (DEBUG_MODE) Serial.println("[DEBUG] megaPool is empty, returning 0");
        #endif
        return 0;
    }

    if (playbackQueue.empty()) {
        #ifdef DEBUG_MODE
        if (DEBUG_MODE) Serial.println("[DEBUG] Queue empty, reshuffling...");
        #endif
        resetPlaybackQueue();
    }

    std::vector<int> skippedCards;
    int foundIndex = -1;

    // Search deck for non-conflicting story
    while (!playbackQueue.empty()) {
        int idx = playbackQueue.back();
        playbackQueue.pop_back();

        if (idx >= megaPool.size()) {
            #ifdef DEBUG_MODE
            if (DEBUG_MODE) {
                Serial.print("[DEBUG] Invalid index "); Serial.print(idx);
                Serial.print(" >= pool size "); Serial.println(megaPool.size());
            }
            #endif
            continue;
        }

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
            #ifdef DEBUG_MODE
            if (DEBUG_MODE) {
                Serial.print("[DEBUG] Selected story idx="); Serial.print(idx);
                Serial.print(" from source "); Serial.print(src);
                Serial.print(" ("); Serial.print(sources[src].name); Serial.println(")");
                Serial.print("[DEBUG] Headline: ");
                Serial.println(megaPool[idx].headline.substring(0, min(60, (int)megaPool[idx].headline.length())));
            }
            #endif
            break; 
        } else {
            #ifdef DEBUG_MODE
            if (DEBUG_MODE) {
                Serial.print("[DEBUG] Skipping idx="); Serial.print(idx);
                Serial.print(" (source "); Serial.print(src); Serial.println(" is forbidden)");
            }
            #endif
            skippedCards.push_back(idx); // Save conflict for later
        }
    }

    // Fallback: If EVERYTHING conflicts, take the first skipped one
    if (foundIndex == -1) {
        #ifdef DEBUG_MODE
        if (DEBUG_MODE) Serial.println("[DEBUG] All stories conflicted with forbidden list");
        #endif
        if (!skippedCards.empty()) {
            foundIndex = skippedCards[0];
            skippedCards.erase(skippedCards.begin());
            #ifdef DEBUG_MODE
            if (DEBUG_MODE) {
                Serial.print("[DEBUG] Using first skipped card idx="); Serial.println(foundIndex);
            }
            #endif
        } else {
            // Should not happen unless pool is empty
            #ifdef DEBUG_MODE
            if (DEBUG_MODE) Serial.println("[DEBUG] No skipped cards, reshuffling queue");
            #endif
            resetPlaybackQueue();
            if(!playbackQueue.empty()) {
                foundIndex = playbackQueue.back();
                playbackQueue.pop_back();
                #ifdef DEBUG_MODE
                if (DEBUG_MODE) {
                    Serial.print("[DEBUG] Selected from fresh queue: idx="); Serial.println(foundIndex);
                }
                #endif
            } else {
                #ifdef DEBUG_MODE
                if (DEBUG_MODE) Serial.println("[DEBUG] Queue still empty after reshuffle, returning 0");
                #endif
                return 0;
            }
        }
    }

    // Return skipped cards to the BOTTOM of the deck
    if (!skippedCards.empty()) {
        #ifdef DEBUG_MODE
        if (DEBUG_MODE) {
            Serial.print("[DEBUG] Returning "); Serial.print(skippedCards.size());
            Serial.println(" skipped cards to bottom of deck");
        }
        #endif
        playbackQueue.insert(playbackQueue.begin(), skippedCards.begin(), skippedCards.end());
    }

    return foundIndex;
}

String cleanText(String raw) {
  #ifdef DEBUG_MODE
  if (DEBUG_MODE) {
    Serial.print("[DEBUG] cleanText input length: "); Serial.println(raw.length());
    Serial.print("[DEBUG] cleanText preview: ");
    Serial.println(raw.substring(0, min(80, (int)raw.length())));
  }
  #endif
  
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
      #ifdef DEBUG_MODE
      if (DEBUG_MODE) Serial.println("[DEBUG] cleanText cropped to max length");
      #endif
  }
  
  #ifdef DEBUG_MODE
  if (DEBUG_MODE) {
    Serial.print("[DEBUG] cleanText output length: "); Serial.println(raw.length());
    Serial.print("[DEBUG] cleanText result: "); Serial.println(raw);
  }
  #endif
  
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
  
  // Additional URL validation
  if (!raw.startsWith("http://") && !raw.startsWith("https://")) {
      return "";  // Invalid protocol
  }
  if (raw.length() > 500) {
      raw = raw.substring(0, 500);  // Truncate suspiciously long URLs
  }
  
  return raw;
}

// Validity Check (Junk Filter)
bool isValidStory(String headline) {
    #ifdef DEBUG_MODE
    if (DEBUG_MODE) {
        Serial.print("[DEBUG] isValidStory checking: ");
        Serial.println(headline.substring(0, min(60, (int)headline.length())));
    }
    #endif
    
    if (headline.length() < 25) {
        #ifdef DEBUG_MODE
        if (DEBUG_MODE) Serial.println("[DEBUG] isValidStory: REJECTED - too short (< 25 chars)");
        #endif
        return false;
    }
    String upper = headline; upper.toUpperCase();
    
    // Hard blocks (Not news)
    if (upper.indexOf("TODAYS HEADLINES") >= 0) { 
        #ifdef DEBUG_MODE
        if (DEBUG_MODE) Serial.println("[DEBUG] isValidStory: REJECTED - TODAYS HEADLINES");
        #endif
        return false;
    }
    if (upper.indexOf("MORNING BRIEFING") >= 0) {
        #ifdef DEBUG_MODE
        if (DEBUG_MODE) Serial.println("[DEBUG] isValidStory: REJECTED - MORNING BRIEFING");
        #endif
        return false;
    }
    if (upper.indexOf("ABOUT US") >= 0) {
        #ifdef DEBUG_MODE
        if (DEBUG_MODE) Serial.println("[DEBUG] isValidStory: REJECTED - ABOUT US");
        #endif
        return false;
    }
    if (upper.indexOf("CONTACT US") >= 0) {
        #ifdef DEBUG_MODE
        if (DEBUG_MODE) Serial.println("[DEBUG] isValidStory: REJECTED - CONTACT US");
        #endif
        return false;
    }
    if (upper.indexOf("LATEST HEADLINES") >= 0) {
        #ifdef DEBUG_MODE
        if (DEBUG_MODE) Serial.println("[DEBUG] isValidStory: REJECTED - LATEST HEADLINES");
        #endif
        return false;
    }
    if (upper.indexOf("EVENING BRIEFING") >= 0) {
        #ifdef DEBUG_MODE
        if (DEBUG_MODE) Serial.println("[DEBUG] isValidStory: REJECTED - EVENING BRIEFING");
        #endif
        return false;
    }
    if (upper.indexOf("DAILY DIGEST") >= 0) {
        #ifdef DEBUG_MODE
        if (DEBUG_MODE) Serial.println("[DEBUG] isValidStory: REJECTED - DAILY DIGEST");
        #endif
        return false;
    }
    if (upper.indexOf("SUBSCRIBE TO") >= 0) {
        #ifdef DEBUG_MODE
        if (DEBUG_MODE) Serial.println("[DEBUG] isValidStory: REJECTED - SUBSCRIBE TO");
        #endif
        return false;
    }
    if (upper.indexOf("SIGN UP") >= 0) {
        #ifdef DEBUG_MODE
        if (DEBUG_MODE) Serial.println("[DEBUG] isValidStory: REJECTED - SIGN UP");
        #endif
        return false;
    }
    if (upper.indexOf("JAVASCRIPT") >= 0) {
        #ifdef DEBUG_MODE
        if (DEBUG_MODE) Serial.println("[DEBUG] isValidStory: REJECTED - JAVASCRIPT");
        #endif
        return false;
    }
    if (upper.indexOf("ACCESS DENIED") >= 0) {
        #ifdef DEBUG_MODE
        if (DEBUG_MODE) Serial.println("[DEBUG] isValidStory: REJECTED - ACCESS DENIED");
        #endif
        return false;
    }
    if (upper.indexOf("404 NOT FOUND") >= 0) {
        #ifdef DEBUG_MODE
        if (DEBUG_MODE) Serial.println("[DEBUG] isValidStory: REJECTED - 404 NOT FOUND");
        #endif
        return false;
    }
    if (upper.indexOf("ERROR") >= 0 && upper.length() < 50) {
        #ifdef DEBUG_MODE
        if (DEBUG_MODE) Serial.println("[DEBUG] isValidStory: REJECTED - ERROR message");
        #endif
        return false;
    }
    if (upper.indexOf("<!DOCTYPE") >= 0) {
        #ifdef DEBUG_MODE
        if (DEBUG_MODE) Serial.println("[DEBUG] isValidStory: REJECTED - HTML DOCTYPE");
        #endif
        return false;
    }

    // Quality blocks (Clickbait/Fluff)
    if (upper.startsWith("HOW TO ")) {
        #ifdef DEBUG_MODE
        if (DEBUG_MODE) Serial.println("[DEBUG] isValidStory: REJECTED - HOW TO");
        #endif
        return false;
    }
    if (upper.startsWith("BEST OF ")) {
        #ifdef DEBUG_MODE
        if (DEBUG_MODE) Serial.println("[DEBUG] isValidStory: REJECTED - BEST OF");
        #endif
        return false;
    }
    if (upper.startsWith("DEALS: ")) {
        #ifdef DEBUG_MODE
        if (DEBUG_MODE) Serial.println("[DEBUG] isValidStory: REJECTED - DEALS");
        #endif
        return false;
    }
    if (upper.startsWith("HOROSCOPE")) {
        #ifdef DEBUG_MODE
        if (DEBUG_MODE) Serial.println("[DEBUG] isValidStory: REJECTED - HOROSCOPE");
        #endif
        return false;
    }
    if (upper.startsWith("WORDLE ")) {
        #ifdef DEBUG_MODE
        if (DEBUG_MODE) Serial.println("[DEBUG] isValidStory: REJECTED - WORDLE");
        #endif
        return false;
    }
    if (upper.startsWith("CROSSWORD ")) {
        #ifdef DEBUG_MODE
        if (DEBUG_MODE) Serial.println("[DEBUG] isValidStory: REJECTED - CROSSWORD");
        #endif
        return false;
    }
    if (upper.startsWith("10 THINGS ")) {
        #ifdef DEBUG_MODE
        if (DEBUG_MODE) Serial.println("[DEBUG] isValidStory: REJECTED - 10 THINGS");
        #endif
        return false;
    }
    if (upper.startsWith("5 THINGS ")) {
        #ifdef DEBUG_MODE
        if (DEBUG_MODE) Serial.println("[DEBUG] isValidStory: REJECTED - 5 THINGS");
        #endif
        return false;
    }
    if (upper.startsWith("TOP ") && upper.indexOf("STORIES") >= 0) {
        #ifdef DEBUG_MODE
        if (DEBUG_MODE) Serial.println("[DEBUG] isValidStory: REJECTED - TOP STORIES");
        #endif
        return false;
    }
    if (upper.startsWith("GALLERY: ")) {
        #ifdef DEBUG_MODE
        if (DEBUG_MODE) Serial.println("[DEBUG] isValidStory: REJECTED - GALLERY");
        #endif
        return false;
    }
    
    // Generic/weak titles
    if (upper.indexOf("QUESTION OF THE") >= 0 && upper.length() < 50) {
        #ifdef DEBUG_MODE
        if (DEBUG_MODE) Serial.println("[DEBUG] isValidStory: REJECTED - Generic question title");
        #endif
        return false;
    }
    if (upper.indexOf("ARCHIVES") >= 0 && upper.length() < 50) {
        #ifdef DEBUG_MODE
        if (DEBUG_MODE) Serial.println("[DEBUG] isValidStory: REJECTED - Archive page");
        #endif
        return false;
    }
    
    #ifdef DEBUG_MODE
    if (DEBUG_MODE) Serial.println("[DEBUG] isValidStory: ACCEPTED");
    #endif
    
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
    if (start < 0) {
        Serial.print("[WARN] Missing tag: "), Serial.println(openTag);
        return "";
    }
    start += strlen(openTag);
    int end = xml.indexOf(closeTag, start);
    if (end < 0 || end <= start) {
        Serial.print("[WARN] Malformed tag: "), Serial.println(closeTag);
        return "";
    }
    return xml.substring(start, end);
}

time_t parseRSSDate(String d) {
  struct tm t = {0};
  d.trim();
  if (d.length() < 20) {
      Serial.print("[WARN] Date too short: "), Serial.println(d);
      return 0;
  }
  
  // Try strptime with standard RFC 2822 format: "Wed, 09 Feb 2026 14:30:45 +0000"
  const char* result = strptime(d.c_str(), "%a, %d %b %Y %H:%M:%S", &t);
  if (!result) {
      Serial.print("[WARN] Date parse failed: "), Serial.println(d);
      return 0;
  }
  
  // Validate parsed values
  if (t.tm_mday < 1 || t.tm_mday > 31) { Serial.println("[WARN] Day out of range"); return 0; }
  if (t.tm_hour < 0 || t.tm_hour > 23) { Serial.println("[WARN] Hour out of range"); return 0; }
  if (t.tm_min < 0 || t.tm_min > 59) { Serial.println("[WARN] Minute out of range"); return 0; }
  if (t.tm_sec < 0 || t.tm_sec > 59) { Serial.println("[WARN] Second out of range"); return 0; }
  if (t.tm_year + 1900 < 2020 || t.tm_year + 1900 > 2100) { Serial.println("[WARN] Year out of range"); return 0; }
  
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
  Serial.println("\n========================================");
  Serial.print("[NewsCore] Fetching: ");
  Serial.println(sources[sourceIdx].name);
  Serial.print("[NewsCore] Source URL: ");
  Serial.println(sources[sourceIdx].url);
  Serial.print("[NewsCore] Is WordPress: ");
  Serial.println(sources[sourceIdx].isWordpress ? "Yes" : "No");
  #ifdef DEBUG_MODE
  if (DEBUG_MODE) {
    Serial.print("[DEBUG] Current pool size: "); Serial.println(megaPool.size());
    Serial.print("[DEBUG] Free heap: "); Serial.println(ESP.getFreeHeap());
  }
  #endif
  
  // Skip sources that have failed too many times in a row
  if (sourceStats[sourceIdx].consecutiveFails > 2) {
      Serial.print("[NewsCore] SKIP - Source too unreliable (failures: ");
      Serial.print(sourceStats[sourceIdx].consecutiveFails);
      Serial.println(")");
      return;
  }
  
  // Per-source tracking
  sourceStats[sourceIdx].lastFetchMs = millis();
  
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
           sourceStats[sourceIdx].fetched++;
           String tempTitle = "", tempDate = "", tempLink = "", tempDesc = "", tempContent = "";
           bool isWp = sources[sourceIdx].isWordpress;
           // WordPress sources have large content blocks; increase buffer for full extraction
           int itemMaxLen = isWp ? 4000 : 1500;
           String itemXml = safeReadUntilEndTagWithTimeout(stream, "</item>", itemMaxLen, ITEM_PARSE_TIMEOUT_MS);
           if (itemXml == "") {
               Serial.print("[DEBUG] Item #"); Serial.print(itemsProcessed); Serial.println(" - Parse timeout");
               sourceStats[sourceIdx].parseErrors++;
               sourceStats[sourceIdx].consecutiveFails++;
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
               
               if (s.url.length() < 12 || s.url.length() > 500 || !s.url.startsWith("http")) {
                   Serial.println("[DEBUG]   REJECTED: Invalid URL");
                   sourceStats[sourceIdx].parseErrors++;
                   sourceStats[sourceIdx].consecutiveFails++;
                   continue;
               }
               // Check for redirect loops or obvious bad URLs
               if (s.url.indexOf("://://") >= 0 || s.url.indexOf("javascript:") >= 0) {
                   Serial.println("[DEBUG]   REJECTED: Malicious URL detected");
                   sourceStats[sourceIdx].parseErrors++;
                   sourceStats[sourceIdx].consecutiveFails++;
                   continue;
               }
               if (s.headline.length() < 15) {
                   Serial.println("[DEBUG]   REJECTED: Headline too short");
                   sourceStats[sourceIdx].parseErrors++;
                   sourceStats[sourceIdx].consecutiveFails++;
                   continue;
               }

               // Local De-Duplication (Same Source Only) - O(1) set lookup
               if (existingHeadlines.find(s.headline) != existingHeadlines.end()) {
                   Serial.println("[DEBUG]   REJECTED: Duplicate");
                   sourceStats[sourceIdx].duplicates++;
                   continue;
               }

               if (!isValidStory(s.headline)) {
                   Serial.println("[DEBUG]   REJECTED: Failed validation filter");
                   sourceStats[sourceIdx].parseErrors++;
                   sourceStats[sourceIdx].consecutiveFails++;
                   continue;
               }

               s.timestamp = parseRSSDate(tempDate);
               if (s.timestamp == 0) {
                   Serial.println("[DEBUG]   REJECTED: Invalid date");
                   sourceStats[sourceIdx].parseErrors++;
                   consecutiveParseFailures++;
                   sourceStats[sourceIdx].consecutiveFails++;
                   continue;
               }
               consecutiveParseFailures = 0;
               sourceStats[sourceIdx].consecutiveFails = 0;
               
               s.timeStr = formatTime(s.timestamp);
               s.sourceIndex = sourceIdx;
               megaPool.push_back(s);
               existingHeadlines.insert(s.headline);
               storiesFound++;
               sourceStats[sourceIdx].accepted++;
               Serial.print("[DEBUG]   ACCEPTED! Stories from this source: "); Serial.println(storiesFound);
               
               if (megaPool.size() >= MAX_POOL_SIZE) break;
           } else {
               Serial.println("[DEBUG]   REJECTED: No title");
               consecutiveParseFailures++;
               sourceStats[sourceIdx].parseErrors++;
               sourceStats[sourceIdx].consecutiveFails++;
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
      
      Serial.println("\n--- Source Fetch Complete ---");
      Serial.print("[NewsCore] Items processed: "); Serial.println(itemsProcessed);
      Serial.print("[NewsCore] Stories added: "); Serial.println(storiesFound);
      Serial.print("[NewsCore] Total pool size now: "); Serial.println(megaPool.size());
      #ifdef DEBUG_MODE
      if (DEBUG_MODE) {
        Serial.print("[DEBUG] Final free heap: "); Serial.println(ESP.getFreeHeap());
        Serial.print("[DEBUG] Source stats - fetched: "); Serial.print(sourceStats[sourceIdx].fetched);
        Serial.print(", accepted: "); Serial.print(sourceStats[sourceIdx].accepted);
        Serial.print(", duplicates: "); Serial.print(sourceStats[sourceIdx].duplicates);
        Serial.print(", parse errors: "); Serial.println(sourceStats[sourceIdx].parseErrors);
      }
      #endif
      Serial.println("========================================\n");
      
      if ((millis() - sourceStart) >= SOURCE_FETCH_TIMEOUT_MS) {
                    Serial.println("[NewsCore] Source fetch timeout.");
            }
    } else {
        Serial.print("HTTP Error: "); Serial.println(httpCode);
        lastSyncFailed = true; 
        sourceStats[sourceIdx].parseErrors++;
        sourceStats[sourceIdx].consecutiveFails++;
    }
    http.end();
  } else {
      Serial.println("Connection Failed.");
      lastSyncFailed = true;
      sourceStats[sourceIdx].parseErrors++;
      sourceStats[sourceIdx].consecutiveFails++;
  }
}

void refreshNewsData(int batchIndex) {
  #ifdef OFFLINE_MODE
  if (OFFLINE_MODE) { return; }
  #endif

  Serial.println("\n\n##########################################");
  Serial.println("###  NEWS REFRESH CYCLE STARTING      ###");
  Serial.println("##########################################");
  Serial.print("[NewsCore] Batch Index: "); Serial.println(batchIndex);
  Serial.print("[NewsCore] Batch Range: Sources "); 
  Serial.print(batchIndex * 6); Serial.print(" - "); Serial.println((batchIndex * 6) + 5);
  
  #ifdef DEBUG_MODE
  if (DEBUG_MODE) {
    Serial.print("[DEBUG] Pre-fetch pool size: "); Serial.println(megaPool.size());
    Serial.print("[DEBUG] Pre-fetch free heap: "); Serial.println(ESP.getFreeHeap());
    Serial.print("[DEBUG] WiFi status: "); 
    Serial.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("[DEBUG] WiFi RSSI: "); Serial.println(WiFi.RSSI());
    }
  }
  #endif
  
  // Reset stats for this batch
  int start = batchIndex * 6;
  int end = start + 6;
  for(int i = start; i < end; i++) {
      sourceStats[i].fetched = 0;
      sourceStats[i].accepted = 0;
      sourceStats[i].duplicates = 0;
      sourceStats[i].parseErrors = 0;
  }
  
  
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
  
  // Exempt Google News aggregators and sources with historically older content
  // 0=Valdosta, 1=Thomasville, 2=Moultrie, 5=Wakulla Sun, 10=WJHG, 11=CNN
  // These return older/incorrectly dated articles due to Google News aggregation
  const int exemptSources[] = {0, 1, 2, 5, 10, 11};
  const int exemptSourceCount = 6;
  
  bool isExempt[30] = {false};
  for(int i = 0; i < exemptSourceCount; i++) {
      isExempt[exemptSources[i]] = true;
  }
  
  Serial.print("[DEBUG] Before age pruning: "); Serial.println(megaPool.size());

  // Prune very old stories, but exempt Google News aggregators
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
  Serial.println("\n--- FINAL POOL STATE ---");
  Serial.print("[NewsCore] Total Stories in Pool: "); Serial.println(megaPool.size());
  Serial.print("[NewsCore] Playback Queue Size: "); Serial.println(playbackQueue.size());
  Serial.print("[NewsCore] Free Heap: "); Serial.println(ESP.getFreeHeap());
  
  #ifdef DEBUG_MODE
  if (DEBUG_MODE) {
    // Log all stories in pool by source
    Serial.println("\n[DEBUG] Stories in pool by source:");
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
    
    // Show a sample of recent stories
    Serial.println("\n[DEBUG] Sample of recent stories (up to 5):");
    int sampleCount = 0;
    for(const auto& s : megaPool) {
        if (sampleCount >= 5) break;
        Serial.print("[DEBUG]   "); Serial.print(sources[s.sourceIndex].name);
        Serial.print(" - "); Serial.println(s.headline);
        Serial.print("[DEBUG]     Time: "); Serial.print(s.timeStr);
        Serial.print(" | URL: "); Serial.println(s.url.substring(0, min(60, (int)s.url.length())));
        sampleCount++;
    }
  }
  #endif

  // Batch summary (per-source stats)
  Serial.println("\n==========================================");
  Serial.println("[SUMMARY] BATCH SOURCE STATISTICS");
  Serial.println("==========================================");
  int totalFetched = 0, totalAccepted = 0, totalDups = 0, totalErrors = 0;
  for (int src = start; src < end; src++) {
      Serial.print("[SUMMARY] Source "); Serial.print(src); Serial.print(" - "); Serial.println(sources[src].name);
      Serial.print("  Fetched: "); Serial.print(sourceStats[src].fetched);
      Serial.print(" | Accepted: "); Serial.print(sourceStats[src].accepted);
      Serial.print(" | Duplicates: "); Serial.print(sourceStats[src].duplicates);
      Serial.print(" | Parse Errors: "); Serial.print(sourceStats[src].parseErrors);
      Serial.print(" | Consecutive Fails: "); Serial.println(sourceStats[src].consecutiveFails);
      if (sourceStats[src].fetched > 0) {
        float acceptRate = (float)sourceStats[src].accepted / sourceStats[src].fetched * 100.0;
        Serial.print("  Accept Rate: "); Serial.print(acceptRate, 1); Serial.println("%");
      }
      totalFetched += sourceStats[src].fetched;
      totalAccepted += sourceStats[src].accepted;
      totalDups += sourceStats[src].duplicates;
      totalErrors += sourceStats[src].parseErrors;
  }
  Serial.println("------------------------------------------");
  Serial.print("[SUMMARY] Batch Totals - Fetched: "); Serial.print(totalFetched);
  Serial.print(" | Accepted: "); Serial.print(totalAccepted);
  Serial.print(" | Duplicates: "); Serial.print(totalDups);
  Serial.print(" | Errors: "); Serial.println(totalErrors);
  if (totalFetched > 0) {
    float overallRate = (float)totalAccepted / totalFetched * 100.0;
    Serial.print("[SUMMARY] Overall Accept Rate: "); Serial.print(overallRate, 1); Serial.println("%");
  }
  Serial.println("==========================================\n");
  Serial.println("###  NEWS REFRESH CYCLE COMPLETE      ###");
  Serial.println("##########################################\n\n");
}