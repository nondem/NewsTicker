A stability-focused RSS news ticker for the ESP32 (32E/CYD) using a 4.0" TFT display.

## Features
- **30 Sources:** Rotates through 5 batches of 6 sources to avoid API blocking.
- **Smart 24h Cleanse:** Automatically reboots daily during idle time to clear RAM.
- **Stability First:** Includes generous timeouts, low-memory guards, and graceful degradation.
- **Production-Optimized:** Configurable debug output, WDT protections in rendering loops, and heap monitoring.
- **Hardware:** ESP32-2432S028R / 4.0" ST7796S Display (CYD variant).

## How to Compile
1. Clone this repository
2. Install [PlatformIO](https://platformio.org/) extension in VS Code
3. Update `platformio.ini` with your upload port (currently `COM7`)
4. Run: `platformio run --target upload`
5. Monitor output: `platformio device monitor --baud 115200`

### First Boot WiFi Configuration
If no saved WiFi credentials are found, the device enters AP mode:
- **Connect to:** `Randys-News-Config`
- **Navigate to:** `http://1.1.1.1` (or your AP's IP)
- **Configure:** SSID and password, then device reboots automatically

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

## Configuration

All user-configurable settings are in `Settings.h`:

```cpp
#define USER_TIMEZONE_HOUR      -5              // EST
#define UPDATE_INTERVAL_MS      900000          // 15 minutes between batches
#define CAROUSEL_INTERVAL_MS    15000           // 15 seconds per headline slide
#define MAX_POOL_SIZE           180             // Max headlines in memory
#define MAX_HEADLINE_LEN        114             // Character limit for display
#define FETCH_LIMIT_PER_SRC     6               // Headlines per source per batch
#define MAX_AGE_SECONDS         129600          // 36 hours - discard old headlines
#define DEBUG_MODE              false           // Set true for verbose Serial output
```

News source definitions and their colors are in `NewsCore.cpp`. All 30 sources rotate through 5 batches of 6 sources each to minimize API blocking.

## User Operations

- **Single Tap**: Generates QR code for currently displayed headlines. Tap to cycle through the 3 visible stories. Long press to return to ticker.
- **Long Press**: Forces download of the next batch of headlines. **Caution:** Excessive use may trigger API rate limiting (24-48 hour blocks).
- **5 Rapid Taps**: Easter egg tribute display.

## Performance & Stability Metrics

- **Memory Usage**: ~80-100KB for 180 headlines in RAM (varies by text length)
- **Fetch Cycle**: ~20-30 seconds for 6 sources (with 20s per-source timeout)
- **Uptime**: Indefinite with automatic 24-hour RAM cleanse
- **Heap Monitoring**: Gracefully reduces collection under 25KB; aborts under 15KB
- **Display Refresh**: ~200-300ms for full screen redraw at 40MHz SPI
- **WiFi**: Auto-reconnect with 10-second timeout; configurable via captive portal
## Recent Optimizations (v50)
The codebase has been optimized for production stability without sacrificing reliability:

- **Debug Output Control**: Conditional Serial logging reduces CPU overhead in production mode
- **Watchdog Protection**: Added WDT resets during long rendering operations to prevent spurious resets
- **Heap Monitoring**: Proactive heap thresholds warn before critical memory pressure
- **Vector Pre-allocation**: News pool reserves full capacity upfront, eliminating runtime reallocations
- **Move Semantics**: String operations use move semantics to reduce allocations
- **Graceful Degradation**: System reduces batch sizes under memory pressure instead of failing
- **Optimized String Handling**: Reduced temporary String copies during text cleaning and validation
- **WiFi Status Caching**: Minimizes radio status checks during fetch cycles
- **SPI Bus Optimization**: 40MHz SPI clock with efficient transaction handling

## Building from Source

### Requirements
- **PlatformIO** (recommended) or Arduino IDE
- **ESP32 Board Support** (espressif32 platform)
- **QRCode Library**: `ricmoo/QRCode` (auto-installed via platformio.ini)

### Compile with PlatformIO
```bash
platformio run --target upload
```

### Monitor Serial Output
```bash
platformio device monitor --baud 115200
```

## Troubleshooting

### Device reboots frequently
- **Check heap**: Monitor serial output for low-memory warnings
- **Reduce MAX_POOL_SIZE** in Settings.h if news sources are verbose
- **Verify power supply**: ESP32 can brown out under high SPI load

### Headlines not updating
- **WiFi issue**: Check serial output for connection errors
- **API rate limiting**: Wait 24-48 hours if you see persistent 429 errors
- **Source misconfiguration**: Verify URLs in NewsCore.cpp are valid RSS feeds

### Display artifacts or visual glitches
- **WDT reset during rendering**: Increase WDT_TIMEOUT_SECONDS or reduce display refresh rate
- **SPI bus contention**: Ensure no other SPI devices interfere

### Serial monitor shows "SYNC ERROR"
- **Network connectivity**: Verify WiFi is properly configured
- **Source unavailable**: Check that at least some news sources are responding

## Architecture Overview

**Main Loop Cycle:**
1. Check timer for update interval
2. If due, fetch new headlines (6 sources per cycle)
3. Display rotation via carousel timer
4. Handle touch input (QR code mode, force refresh)
5. Automatic 24-hour RAM cleanse

**Memory Management:**
- **megaPool**: Vector of Story structs (~180 max)
- **playbackQueue**: Shuffled deck for carousel rotation
- **Heap Guards**: Monitor ESP.getFreeHeap() continuously

**Networking:**
- WiFi credentials stored in Preferences (flash)
- Captive portal on first boot or after reset
- Per-source 20s timeout; global 10s parse timeout

## License

Enjoy the ticker! Modify as needed for your use case.