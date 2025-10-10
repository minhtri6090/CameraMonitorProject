#include "config.h"
#include "wifi_manager.h"
#include "camera_handler.h"
#include "web_server.h"
#include "blynk_handler.h"
#include "audio_handler.h"
#include "sensors_handler.h"

bool sdAudioInitialized = false;
bool welcomeAudioPlayed = false;
bool wifiConnectionStarted = false;
bool wifiResultProcessed = false;
bool pirInitialized = false;
bool servoInitialized = false;

bool needPlaySuccessAudio = false;
unsigned long wifiStartTime = 0;
const unsigned long WIFI_TIMEOUT = 30000;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Check PSRAM
    if (!psramFound()) {
        Serial.println("ERROR: PSRAM NOT FOUND!");
        while(1) delay(1000);
    }

    EEPROM.begin(512);

    loadCredentials();

    initializeBuffers();

    initializeCamera();
    
    Serial.println("=== SETUP COMPLETE - ENTERING MAIN LOOP ===");
}

void loop() {
    handleAudioLoop();

    if (!sdAudioInitialized) {
        Serial.println("[SYSTEM] Initializing SD Card and Audio...");
        initializeSDCard();
        initializeAudio();
        delay(500);
        playAudio(AUDIO_HELLO);
        
        sdAudioInitialized = true;
        welcomeAudioPlayed = true;
        return;
    }

    if (welcomeAudioPlayed && !wifiConnectionStarted && !isAudioPlaying()) {
        Serial.println("[WIFI] Starting WiFi connection...");
        delay(1000);
        
        initializeWiFi();
        wifiConnectionStarted = true;
        wifiStartTime = millis();
        return;
    }

    if (wifiConnectionStarted && !wifiResultProcessed) {
        handleWiFiLoop();

        if (wifiState == WIFI_STA_OK && !isAudioPlaying()) {
            Serial.println("[AUDIO] WiFi connected - Playing success audio");
            playAudio(AUDIO_WIFI_SUCCESS);
            delay(100);
            while (isAudioPlaying()) { 
                handleAudioLoop(); 
                yield(); 
                delay(50); 
            }
            
            wifiResultProcessed = true;

            if (!servoInitialized) {
                Serial.println("[SERVO] Initializing servos after WiFi success...");
                initializeServos();
                servoInitialized = true;
            }
            return;
        }

        if ((millis() - wifiStartTime > WIFI_TIMEOUT) && wifiState != WIFI_STA_OK && !isAudioPlaying()) {
            Serial.println("[AUDIO] Initial WiFi FAILED - Playing failure audio...");
            
            playAudio(AUDIO_WIFI_FAILED);
            delay(100);
            while (isAudioPlaying()) { 
                handleAudioLoop(); 
                yield(); 
                delay(50); 
            }
            delay(2000);
            
            wifiResultProcessed = true;
            return;
        }
    }

    if (wifiResultProcessed && !pirInitialized) {
        if (wifiState == WIFI_STA_OK) {
            Serial.println("[SENSORS] Initializing sensors (PIR + LDR)...");
            initializeSensors();
            systemReady = true; 
        } else {
            Serial.println("[SENSORS] WiFi failed - Sensors disabled");
        }
        
        pirInitialized = true;
        
        Serial.println("=== SYSTEM READY ===");
        if (wifiState == WIFI_AP_MODE) {
            Serial.printf("AP Config: http://%s/\n", WiFi.softAPIP().toString().c_str());
        } else {
            Serial.printf("Camera: http://%s/\n", WiFi.localIP().toString().c_str());
        }
        return;
    }

    if (needPlaySuccessAudio && wifiState == WIFI_STA_OK && !isAudioPlaying()) {
        
        Serial.println("[AUDIO] Web-initiated WiFi connection - Playing success audio");
        playAudio(AUDIO_WIFI_SUCCESS);
        delay(100);
        while (isAudioPlaying()) { 
            handleAudioLoop(); 
            yield(); 
            delay(50); 
        }
        needPlaySuccessAudio = false;

        if (!servoInitialized) {
            Serial.println("[SERVO]  Initializing servos...");
            initializeServos();
            servoInitialized = true;
        }

        if (!systemReady) {
            Serial.println("[SENSORS] bInitializing sensors...");
            initializeSensors();
            systemReady = true;
        }
        return;
    }

    if (pirInitialized || systemReady) {
        handleWebServerLoop();
        handleWiFiLoop();
        
        if (systemReady && wifiState == WIFI_STA_OK) {
            handleMotionLoop();
            handleLDRLoop();
            handleBlynkLoop();
        }
    }

    if (systemReady) {
        static unsigned long lastSensorDebug = 0;
        if (millis() - lastSensorDebug > 10000) {
            lastSensorDebug = millis();
            getSensorsStatus();
        }
    }
    
    delay(10);
}