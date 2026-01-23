#include "RandyNet.h"

// ==========================================================
//   CONSTRUCTOR
// ==========================================================
RandyNet::RandyNet(const char* apName, const char* apPass) : _server(80) {
    _apName = apName;
    _apPass = apPass;
}

// ==========================================================
//   AUTO CONNECT
// ==========================================================
bool RandyNet::autoConnect(unsigned long timeoutSeconds, void (*onPortalStart)()) {
    Serial.println("\n[RandyNet] Starting...");

    _prefs.begin("wifi-config", false);

    String ssid = _prefs.getString("ssid", "");
    String pass = _prefs.getString("pass", "");

    if (ssid.length() > 0) {
        Serial.print("[RandyNet] Found saved credentials for: ");
        Serial.println(ssid);
        Serial.print("[RandyNet] Connecting");

        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), pass.c_str());

        int retry = 0;
        while (WiFi.status() != WL_CONNECTED && retry < 30) {
            delay(500);
            Serial.print(".");
            retry++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\n[RandyNet] Connected! IP: " + WiFi.localIP().toString());
            return true; 
        } else {
            Serial.println("\n[RandyNet] Connection Failed.");
        }
    } else {
        Serial.println("[RandyNet] No saved credentials.");
    }

    // --- START AP MODE ---
    Serial.println("[RandyNet] Starting Config Portal...");
    if (onPortalStart) onPortalStart();

    WiFi.mode(WIFI_AP);
    
    // [FIX] Force IP to 1.1.1.1
    WiFi.softAPConfig(IPAddress(1,1,1,1), IPAddress(1,1,1,1), IPAddress(255,255,255,0));
    
    if (_apPass) WiFi.softAP(_apName, _apPass);
    else WiFi.softAP(_apName);

    Serial.print("[RandyNet] AP IP: ");
    Serial.println(WiFi.softAPIP());

    _dnsServer.start(53, "*", WiFi.softAPIP());

    startConfigPortal();

    unsigned long startTime = millis();
    while (millis() - startTime < timeoutSeconds * 1000) {
        _dnsServer.processNextRequest();
        _server.handleClient();
        delay(10);
    }

    Serial.println("[RandyNet] Timeout. Rebooting...");
    ESP.restart();
    return false;
}

void RandyNet::startConfigPortal() {
    _server.on("/", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/save", HTTP_POST, [this]() { handleSave(); });
    _server.onNotFound([this]() { handleRoot(); });
    _server.begin();
}

void RandyNet::handleRoot() {
    String html = getHead();
    html += "<h2>WiFi Configuration</h2>";
    html += "<p>Current AP IP: <b>" + WiFi.softAPIP().toString() + "</b></p>";
    html += "<form action='/save' method='POST'>";
    html += "<input type='text' name='ssid' placeholder='WiFi Name (SSID)'>";
    html += "<input type='password' name='pass' placeholder='Password'>";
    html += "<button type='submit'>Save & Connect</button>";
    html += "</form>";
    html += getTail();
    _server.send(200, "text/html", html);
}

void RandyNet::handleSave() {
    String ssid = _server.arg("ssid");
    String pass = _server.arg("pass");

    if (ssid.length() > 0) {
        _prefs.putString("ssid", ssid);
        _prefs.putString("pass", pass);
        
        String html = getHead();
        html += "<h2>Saved!</h2>";
        html += "<p>Rebooting now...</p>";
        html += getTail();
        _server.send(200, "text/html", html);
        
        delay(1000);
        ESP.restart();
    } else {
        _server.send(200, "text/plain", "Error: SSID cannot be empty.");
    }
}

// ==========================================================
//   UTILITIES
// ==========================================================
void RandyNet::resetSettings() {
    _prefs.begin("wifi-config", false);
    _prefs.clear(); 
    _prefs.end();
    Serial.println("[RandyNet] Settings Wiped.");
}

String RandyNet::getIP() {
    return WiFi.localIP().toString();
}

// ==========================================================
//   HTML HELPERS
// ==========================================================
String RandyNet::getHead() {
    return R"(
<!DOCTYPE html><html><head>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<style>
  body { font-family: sans-serif; text-align: center; background: #222; color: #fff; padding: 20px; }
  h2 { color: #00ccff; }
  input, button { width: 100%; padding: 12px; margin: 8px 0; border-radius: 5px; border: none; }
  input { background: #333; color: white; border: 1px solid #444; }
  button { background: #00ccff; color: #000; font-weight: bold; cursor: pointer; }
  .list { text-align: left; margin-bottom: 20px; background: #333; border-radius: 5px; padding: 10px; }
</style>
</head><body>
)";
}

String RandyNet::getTail() {
    return "</body></html>";
}