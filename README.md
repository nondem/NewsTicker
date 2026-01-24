A stability-focused RSS news ticker for the ESP32 (32E/CYD) using a 4.0" TFT display.

## Features
- **18 Sources:** Rotates through 3 batches of 6 sources to avoid API blocking.
- **Smart 24h Cleanse:** Automatically reboots daily during idle time to clear RAM.
- **Stability First:** Includes 10s WiFi timeouts and low-memory guards.
- **Hardware:** ESP32-2432S028R / 4.0" ST7796S Display.

## How to Compile
1. Download the latest Release from the sidebar.
2. Open `NewsTickerGen6.ino` in Arduino IDE.
3. Install required libraries (TFT_eSPI, ArduinoJson, etc).
4. Flash to ESP32.

This code is working and stable on this hardware:
https://www.lcdwiki.com/4.0inch_ESP32-32E_Display
<br>Wifi config uses captive portal triggered when no connection is found on boot.<br><br>
<img width="1740" height="1167" alt="image" src="https://github.com/user-attachments/assets/8901721a-55c3-4bca-bd4c-8aa2689effbc" />
<br>
News sources are google RSS feeds...configurable in NewsCore.cpp<br>
Examples:<br>
{ "NEWSWEEK",      "https://news.google.com/rss/search?q=site:newsweek.com",      WHITE,  RED,      WHITE },<br>
{ "REUTERS",     "https://news.google.com/rss/search?q=site:reuters.com",   ORANGE, CHARCOAL,WHITE },<br>
{ "ASSOC. PRESS","https://news.google.com/rss/search?q=site:apnews.com",    BLACK, GOLD,    BLACK }, <br>
<br>

Operations are configured in settings.h including Banner rotation & redraw speed.<br>
Headline downloads are in phases to reduce potential blocking by google news. <br>
Headlines are weighted by age to determine order & Headlines with publication dates over 36 hours old are discarded automatically.<br>

Operations:<br>
Single tap generates QRCode links currently displayed headlines - Tap to cycle through them and Long press to return to ticker.<br>
Long press: Force download of the next batch of headlines. CAUTION: Do this too much at once and you might get blocked for a day or two.<br>
