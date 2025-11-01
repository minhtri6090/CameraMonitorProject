#include "config.h"
#include "wifi_manager.h"
#include "camera_handler.h"
#include "web_server.h"
#include "blynk_handler.h"
#include "audio_handler.h"
#include "sensors_handler.h"
#include "security_system.h"

bool sdAudioInitialized = false;
bool welcomeAudioPlayed = false;
bool wifiConnectionStarted = false;
bool wifiResultProcessed = false;
bool pirInitialized = false;
bool servoInitialized = false;
bool securitySystemInitialized = false;

bool needPlaySuccessAudio = false;
unsigned long wifiStartTime = 0;
const unsigned long WIFI_TIMEOUT = 30000;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    if (!psramFound()) {
        Serial.println("ERROR: PSRAM NOT FOUND!");
        while(1) delay(1000);
    }

    EEPROM.begin(512);
    loadCredentials();
    initializeBuffers();
    initializeCamera();
}

void loop() {
    handleAudioLoop();

    if (!sdAudioInitialized) {
        initializeSDCard();
        initializeAudio();
        delay(500);
        playAudio(AUDIO_HELLO);
        
        sdAudioInitialized = true;
        welcomeAudioPlayed = true;
        return;
    }

    if (welcomeAudioPlayed && !wifiConnectionStarted && !isAudioPlaying()) {
        initializeWiFi();
        wifiConnectionStarted = true;
        wifiStartTime = millis();
        return;
    }

    if (wifiConnectionStarted && !wifiResultProcessed) {
        handleWiFiLoop();

        if (wifiState == WIFI_STA_OK && !isAudioPlaying()) {
            playAudio(AUDIO_WIFI_SUCCESS);
            delay(100);
            while (isAudioPlaying()) { 
                handleAudioLoop(); 
                yield(); 
                delay(50); 
            }
            
            wifiResultProcessed = true;

            if (!servoInitialized) {
                initializeServos();
                servoInitialized = true;
            }
            return;
        }

        if ((millis() - wifiStartTime > WIFI_TIMEOUT) && wifiState != WIFI_STA_OK && !isAudioPlaying()) {
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
            initializeSensors();
            systemReady = true; 
        }
        
        pirInitialized = true;
        return;
    }
    
    if (pirInitialized && !securitySystemInitialized) {
        initSecuritySystem();
        securitySystemInitialized = true;
        return;
    }

    if (needPlaySuccessAudio && wifiState == WIFI_STA_OK && !isAudioPlaying()) {
        playAudio(AUDIO_WIFI_SUCCESS);
        delay(100);
        while (isAudioPlaying()) { 
            handleAudioLoop(); 
            yield(); 
            delay(50); 
        }
        needPlaySuccessAudio = false;

        if (!servoInitialized) {
            initializeServos();
            servoInitialized = true;
        }

        if (!systemReady) {
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
            
            if (securitySystemInitialized) {
                handleSecuritySystem();
            }
        }
    }
    
    delay(10);
}