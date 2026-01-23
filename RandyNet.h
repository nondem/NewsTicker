#ifndef RANDYNET_H
#define RANDYNET_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>

class RandyNet {
public:
    RandyNet(const char* apName, const char* apPass = NULL);
    bool autoConnect(unsigned long timeoutSeconds = 180, void (*onPortalStart)() = NULL);
    void resetSettings();
    String getIP();

private:
    const char* _apName;
    const char* _apPass;
    WebServer _server;
    DNSServer _dnsServer;
    Preferences _prefs;

    void startConfigPortal();
    void handleRoot();  
    void handleSave();  
    String getHead();   
    String getTail();   
};

#endif