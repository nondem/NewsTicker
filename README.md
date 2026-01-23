This code is working and stable on this hardware:
https://www.lcdwiki.com/4.0inch_ESP32-32E_Display

News Ticker that works on bare-bones CYD hardware. Should work on most variants with some tweaking.
<img width="1740" height="1167" alt="image" src="https://github.com/user-attachments/assets/8901721a-55c3-4bca-bd4c-8aa2689effbc" />

News sources are google RSS feeds...configurable in NewsCore.cpp
Examples:
{ "NEWSWEEK",      "https://news.google.com/rss/search?q=site:newsweek.com",      WHITE,  RED,      WHITE },
  { "REUTERS",     "https://news.google.com/rss/search?q=site:reuters.com",   ORANGE, CHARCOAL,WHITE },
  { "ASSOC. PRESS","https://news.google.com/rss/search?q=site:apnews.com",    BLACK, GOLD,    BLACK }, 

  Note the banner color definitions are configurable.
