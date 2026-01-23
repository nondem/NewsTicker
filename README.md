This code is working and stable on this hardware:
https://www.lcdwiki.com/4.0inch_ESP32-32E_Display

News Ticker that works on bare-bones CYD hardware. Should work on most variants with some tweaking.
<img width="1740" height="1167" alt="image" src="https://github.com/user-attachments/assets/8901721a-55c3-4bca-bd4c-8aa2689effbc" />
<br>
News sources are google RSS feeds...configurable in NewsCore.cpp<br>
Examples:<br>
{ "NEWSWEEK",      "https://news.google.com/rss/search?q=site:newsweek.com",      WHITE,  RED,      WHITE },<br>
{ "REUTERS",     "https://news.google.com/rss/search?q=site:reuters.com",   ORANGE, CHARCOAL,WHITE },<br>
{ "ASSOC. PRESS","https://news.google.com/rss/search?q=site:apnews.com",    BLACK, GOLD,    BLACK }, <br>
<br>
Downloads the latest 6 headlines from 12 news sources.<br>
Sources are configured in settings.h as well as Banner rotation & redraw speed.<br>
Headline downloads are in phases to reduce potential blocking by google news. <br>
Loads 36 headlines from 6 sources on boot and waits 5 minutes to pull another 36 from the other 6 sources. <br>
Downloads 36 updated headlines from 6 of the 12 sources every 20 minutes.<br>
Wifi config uses captive portal triggered when no connection is found on boot.<br><br>
Operations:<br>
Single tap generates QRCode link for currently displayed headlines - Long press to return to ticker.<br>
Long press: Download next back of headlines.
