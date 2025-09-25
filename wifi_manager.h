#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "config.h"

extern const char* AP_SSID;
extern const char* AP_PASSWORD;
extern const IPAddress AP_IP;
extern const int AP_CHANNEL;
extern const int AP_MAX_CONN;
extern const bool AP_HIDDEN;

extern const char* DEFAULT_SSID;
extern const char* DEFAULT_PASSWORD;

extern WiFiState wifiState;
extern String savedSSID;
extern String savedPassword;
extern bool connecting;
extern String connectingSSID;
extern String connectingPassword;
extern unsigned long connectStartTime;
extern const unsigned long connectTimeout;
extern bool needRestartStream;

void loadCredentials();
void saveCredentials(String ssid, String password);
String readEEPROM(int offset, int maxLen);
void writeEEPROM(int offset, int maxLen, String value);
void connectWiFiSTA(String ssid, String password);
void startAPConfigPortal();
void initializeWiFi();
void handleWiFiLoop();

#endif