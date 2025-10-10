#include "wifi_manager.h"
#include "web_server.h"
#include "camera_handler.h"
#include "blynk_handler.h"
#include "audio_handler.h"

extern WebServer server;
extern bool serverRunning;

const char* AP_SSID = "Camera Monitor";
const char* AP_PASSWORD = "12345678";
const IPAddress AP_IP(192, 168, 4, 1);
const int AP_CHANNEL = 1;
const int AP_MAX_CONN = 4;
const bool AP_HIDDEN = false;

WiFiState wifiState = WIFI_AP_MODE;
bool needRestartStream = false;
bool connecting = false;
String connectingSSID = "";
String connectingPassword = "";
unsigned long connectStartTime = 0;
const unsigned long connectTimeout = 30000;

String savedSSID = "";
String savedPassword = "";

static int connectionAttempts = 0;
static const int maxConnectionAttempts = 2;
static unsigned long lastLogTime = 0;

extern bool needPlaySuccessAudio;

void loadCredentials() {
    //đọc dữu liệu từ EEPROM
    savedSSID = readEEPROM(0, 32);
    savedPassword = readEEPROM(32, 64);
    //kiểm tra nếu không không có có hoặc sai má dài thì sẽ sóa các kí tự trong EEPROM
    if (savedSSID.length() == 0 || savedSSID.length() > 32 || savedPassword.length() > 64) {
        Serial.println("[WIFI] Invalid credentials, clearing...");
        savedSSID = "";
        savedPassword = "";
        for (int i = 0; i < 96; i++) {
            EEPROM.write(i, 0);
        }
        EEPROM.commit();
    }
    //kiểm tra kí tự có phù hơp trong ASCII
    for (int i = 0; i < savedSSID.length(); i++) {
        if (savedSSID[i] < 32 || savedSSID[i] > 126) {
            Serial.println("[WIFI] Non-printable characters, clearing...");
            savedSSID = "";
            savedPassword = "";
            break;
        }
    }
    
    Serial.printf("[WIFI] Loaded SSID: '%s' (length: %d)\n", savedSSID.c_str(), savedSSID.length());
}

void saveCredentials(String ssid, String password) {
    if (ssid.length() > 32 || password.length() > 64) {
        Serial.println("[WIFI] Credentials too long");
        return;
    }
    
    writeEEPROM(0, 32, ssid);
    writeEEPROM(32, 64, password);
    EEPROM.commit();
    
    savedSSID = ssid;
    savedPassword = password;
    
    Serial.printf("[WIFI] Saved SSID: '%s'\n", ssid.c_str());
}

String readEEPROM(int offset, int maxLen) {
    String value = "";
    for (int i = 0; i < maxLen; ++i) {
        char c = EEPROM.read(offset + i);
        if (c == 0) break;
        if (c >= 32 && c <= 126) {
            value += c;
        } else {
            break;
        }
    }
    return value;
}

void writeEEPROM(int offset, int maxLen, String value) {
    for (int i = 0; i < maxLen; ++i) {
        if (i < value.length()) {
            EEPROM.write(offset + i, value[i]);
        } else {
            EEPROM.write(offset + i, 0);
        }
    }
}

void connectWiFiSTA(String ssid, String password) {
    if (millis() - lastLogTime > 5000) {
        Serial.printf("[WIFI] Connecting to: %s\n", ssid.c_str());
        lastLogTime = millis();
    }
    
    connectionAttempts++;

    if (wifiState == WIFI_AP_MODE) {
        Serial.println("[WIFI] Stopping AP mode...");
        if (serverRunning) {
            stopMJPEGStreamingServer();
            delay(500);
        }
        WiFi.softAPdisconnect(true);
        delay(1000);
    }

    WiFi.mode(WIFI_STA);
    delay(500);
    
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    WiFi.setSleep(false);
    
    WiFi.disconnect(true);
    delay(500);
    
    WiFi.begin(ssid.c_str(), password.c_str());
    
    connecting = true;
    connectStartTime = millis();
    connectingSSID = ssid;
    connectingPassword = password;
}

void handleSuccessfulConnection() {
    Serial.println("[WIFI] Connected successfully!");
    Serial.printf("[WIFI] IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WIFI] RSSI: %d dBm\n", WiFi.RSSI());
    
    connecting = false;
    connectionAttempts = 0;
    wifiState = WIFI_STA_OK;

    WiFi.mode(WIFI_STA);
    delay(500);

    startMJPEGStreamingServer();
    start_stream_if_needed();
    initializeBlynk();

    extern bool wifiConnectionStarted;
    extern bool wifiResultProcessed;

    if (wifiConnectionStarted && wifiResultProcessed) {
        Serial.println("[WIFI] This is a web-initiated connection - setting audio flag");
        needPlaySuccessAudio = true;
    } else {
        Serial.println("[WIFI] This is initial connection - main loop will handle audio");
    }
}

void handleFailedConnection() {
    Serial.println("[WIFI] Connection failed!");
    connecting = false;
    
    if (connectionAttempts < maxConnectionAttempts) {
        Serial.printf("[WIFI] Retry %d/%d\n", connectionAttempts + 1, maxConnectionAttempts);
        delay(2000);
        connectWiFiSTA(connectingSSID, connectingPassword);
        return;
    }
    
    Serial.println("[WIFI] All attempts failed - Starting AP mode");
    connectionAttempts = 0;
    wifiState = WIFI_AP_MODE;
    
    startAPConfigPortal();
}

void startAPConfigPortal() {
    Serial.println("[AP] Starting Configuration Portal...");

    if (serverRunning) {
        stopMJPEGStreamingServer();
        delay(500);
    }
    
    stop_stream_if_needed();

    WiFi.disconnect(true);
    delay(1000);

    WiFi.mode(WIFI_AP);
    delay(1000);

    bool configOK = WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
    if (!configOK) {
        Serial.println("[AP] Config failed!");
    }

    bool apOK = WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, AP_HIDDEN, AP_MAX_CONN);
    if (!apOK) {
        Serial.println("[AP] Start failed! Restarting...");
        delay(3000);
        ESP.restart();
    }
    
    delay(2000);

    IPAddress apIP = WiFi.softAPIP();
    if (apIP == IPAddress(0, 0, 0, 0)) {
        Serial.println("[AP] IP assignment failed! Restarting...");
        delay(3000);
        ESP.restart();
    }

    WiFi.setSleep(false);

    startAPWebServer();
    
    wifiState = WIFI_AP_MODE;
    
    Serial.println("[AP] Successfully started!");
    Serial.printf("[AP] SSID: %s\n", AP_SSID);
    Serial.printf("[AP] Password: %s\n", AP_PASSWORD);
    Serial.printf("[AP] IP: %s\n", apIP.toString().c_str());
    Serial.printf("[AP] URL: http://%s/\n", apIP.toString().c_str());

    Serial.printf("[AP] Mode: %d\n", WiFi.getMode());
    Serial.printf("[AP] Status: %d\n", WiFi.status());
}

void initializeWiFi() {
    Serial.println("[WIFI] Initializing WiFi...");
    
    if (savedSSID.length() == 0) {
        Serial.println("[WIFI] No saved credentials - starting AP mode");
        startAPConfigPortal();
        return;
    }
    
    Serial.printf("[WIFI] Found saved credentials for: %s\n", savedSSID.c_str());
    connectWiFiSTA(savedSSID, savedPassword);
}

void handleWiFiLoop() {
    static String lastProcessedSSID = "";
    if (connecting && connectingSSID.length() > 0 && connectingSSID != lastProcessedSSID) {
        Serial.printf("[WIFI] Web-initiated connection: %s\n", connectingSSID.c_str());
        lastProcessedSSID = connectingSSID;
        connectWiFiSTA(connectingSSID, connectingPassword);
        return;
    }

    if (connecting) {
        unsigned long elapsed = millis() - connectStartTime;
        wl_status_t status = WiFi.status();
        
        if (status == WL_CONNECTED) {
            handleSuccessfulConnection();
            lastProcessedSSID = "";
            return;
        }
        
        if (elapsed > connectTimeout) {
            Serial.printf("[WIFI] Timeout (%d seconds)\n", connectTimeout / 1000);
            handleFailedConnection();
            lastProcessedSSID = "";
            return;
        }
        
        if (status == WL_NO_SSID_AVAIL || status == WL_CONNECT_FAILED) {
            Serial.printf("[WIFI] Connection error: %d\n", status);
            handleFailedConnection();
            lastProcessedSSID = "";
            return;
        }
    }

    if (wifiState == WIFI_STA_OK) {
        static unsigned long lastCheck = 0;
        if (millis() - lastCheck > 30000) {
            lastCheck = millis();
            
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("[WIFI] Connection lost!");
                wifiState = WIFI_AP_MODE;
                startAPConfigPortal();
            }
        }
    }
}
