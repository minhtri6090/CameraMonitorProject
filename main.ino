#include "config.h"
#include "wifi_manager.h"
#include "camera_handler.h"
#include "web_server.h"
#include "blynk_handler.h"
#include "audio_handler.h"
#include "sensors_handler.h"
#include "security_system.h"  // Thêm header file mới

bool sdAudioInitialized = false;
bool welcomeAudioPlayed = false;
bool wifiConnectionStarted = false;
bool wifiResultProcessed = false;
bool pirInitialized = false;
bool servoInitialized = false;
bool securitySystemInitialized = false;  // Biến mới để kiểm tra khởi tạo hệ thống bảo mật

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

    // TẮT sensor debug - chỉ giữ Serial commands
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        
        if (command == "mqtt_status") {
            Serial.println("=== MQTT STATUS ===");
            Serial.printf("Connected: %s\n", mqttConnected ? "YES" : "NO");
            Serial.printf("Client state: %d\n", mqttClient.state());
            Serial.printf("WiFi status: %d\n", WiFi.status());
            Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
            Serial.println("==================");
        }
        else if (command == "mqtt_test") {
            String testMessage = "Manual test from ESP32 - " + String(millis());
            publishMQTTStatus(testMessage.c_str());
        }
        else if (command == "family_test") {
            onFamilyMemberDetected();
        }
        else if (command == "test_callback") {
            String testMessage = "{\"event\":\"family_member_detected\",\"user_name\":\"TestUser\",\"confidence\":0.95}";
            mqttCallback((char*)MQTT_TOPIC_FAMILY_DETECT, 
                         (byte*)testMessage.c_str(), 
                         testMessage.length());
        }
        else if (command == "help") {
            Serial.println("Available commands:");
            Serial.println("  mqtt_status - Show MQTT status");
            Serial.println("  mqtt_test - Test MQTT publish");
            Serial.println("  family_test - Simulate family detection");
            Serial.println("  test_callback - Test MQTT callback directly");
        }
    }
    
    delay(10);
}