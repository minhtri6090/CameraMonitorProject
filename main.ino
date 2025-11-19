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

void setup() 
{
    Serial.begin(115200);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    if (!psramFound()) 
    {
        Serial.println("ERROR: PSRAM NOT FOUND!");
        while(1) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    EEPROM.begin(512);
    loadCredentials();
    initializeBuffers();
    initializeCamera();
}

void loop() 
{
    if (!sdAudioInitialized) 
    {
        initializeSDCard();
        initializeAudio();
        vTaskDelay(pdMS_TO_TICKS(500));
        playAudio(AUDIO_HELLO);
        
        sdAudioInitialized = true;
        welcomeAudioPlayed = true;
        return;
    }
    handleAudioLoop();

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
            vTaskDelay(pdMS_TO_TICKS(100));
            while (isAudioPlaying()) { 
                handleAudioLoop(); 
                yield(); 
                vTaskDelay(pdMS_TO_TICKS(50));
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
            vTaskDelay(pdMS_TO_TICKS(100));
            while (isAudioPlaying()) { 
                handleAudioLoop(); 
                yield(); 
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
            
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
        vTaskDelay(pdMS_TO_TICKS(100));
        while (isAudioPlaying()) { 
            handleAudioLoop(); 
            yield(); 
            vTaskDelay(pdMS_TO_TICKS(100));
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
    
    vTaskDelay(pdMS_TO_TICKS(10));
}