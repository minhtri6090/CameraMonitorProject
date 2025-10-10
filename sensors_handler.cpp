#include "sensors_handler.h"
#include "audio_handler.h"
#include "security_system.h"

bool systemReady = false;
bool motionDetected = false;
unsigned long lastMotionTime = 0;
const unsigned long motionCooldown = 20000;

int radarState = LOW; 
int radarVal = 0; 
unsigned long motionStartTime = 0;
bool motionInProgress = false;

int ldrValue = 0;
bool isDark = false;
bool ledState = false;
unsigned long lastLDRRead = 0;

void IRAM_ATTR radarISR() {
    if (systemReady && (millis() - lastMotionTime > motionCooldown)) {
        motionDetected = true;
    }
}

void initializeSensors() {
    Serial.println("[SENSORS] Initializing all sensors...");

    Serial.println("[LD2410C] Initializing LD2410C radar sensor...");
    pinMode(PIR_PIN, INPUT);
    detachInterrupt(digitalPinToInterrupt(PIR_PIN));

    Serial.println("[LDR] Initializing LDR sensor and LED control...");

    pinMode(LDR_PIN, INPUT);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW); 
    
    Serial.printf("[LDR] LDR pin: %d (ADC)\n", LDR_PIN);
    Serial.printf("[LDR] LED  pin: %d (Digital Output)\n", LED_PIN);

    readLDRSensor();
    Serial.printf("[LDR] Initial LDR value: %d, Environment: %s\n", 
                  ldrValue, isDark ? "DARK" : "BRIGHT");

    radarVal = digitalRead(PIR_PIN);
    radarState = LOW;
    motionDetected = false;
    motionInProgress = false;
    lastMotionTime = millis();
    
    if (radarVal == LOW) {
        attachInterrupt(digitalPinToInterrupt(PIR_PIN), radarISR, RISING);
        Serial.println("[LD2410C] Radar sensor ready");
    }
    
    Serial.println("[SENSORS] All sensors initialized successfully:");
}

void readLDRSensor() {
    ldrValue = analogRead(LDR_PIN);
 
    if (!isDark && (ldrValue < LDR_DARK_THRESHOLD)) {
        isDark = true;
        controlLED(true);
        Serial.printf("[LDR] Environment is DARK (value: %d) - LED ON\n", ldrValue);
    } else if (isDark && ldrValue > LDR_BRIGHT_THRESHOLD) {
        isDark = false;
        controlLED(false);
        Serial.printf("[LDR] Environment is BRIGHT (value: %d) - LED OFF\n", ldrValue);
    }
}

void controlLED(bool turnOn) {
    if (turnOn != ledState) {
        ledState = turnOn;
        digitalWrite(LED_PIN, ledState ? HIGH : LOW);
        Serial.printf("[LED] LED %s \n", ledState ? "ON" : "OFF");
    }
}

void handleLDRLoop() {
    if (systemReady && (millis() - lastLDRRead >= LDR_READ_INTERVAL)) {
        lastLDRRead = millis();
        readLDRSensor();
    }
}

void handleMotionLoop() {
    if (systemReady) {
        radarVal = digitalRead(PIR_PIN);
        
        if (radarVal == LOW) {
            if (radarState == LOW) {
                if (millis() - lastMotionTime > motionCooldown) {
                    Serial.println("[MOTION] LD2410C Motion detected!");
                    lastMotionTime = millis();
                    motionStartTime = millis();
                    radarState = HIGH;
                    motionInProgress = true;

                    // Phát âm thanh cảnh báo
                    if (!isAudioPlaying()) {
                        playAudio(AUDIO_MOTION_DETECTED);
                    }
                    
                    // Gửi thông báo phát hiện chuyển động qua MQTT (nếu đã khởi tạo)
                    if (mqttConnected) {
                        StaticJsonDocument<200> doc;
                        doc["event"] = "motion_detected";
                        doc["timestamp"] = millis();
                        
                        char buffer[256];
                        serializeJson(doc, buffer);
                        
                        mqttClient.publish(MQTT_TOPIC_ALERT, buffer);
                    }
                    
                    // Đặt mức cảnh báo ban đầu khi phát hiện chuyển động
                    if (currentAlertLevel == ALERT_NONE) {
                        // Cảnh báo cấp độ 1 (sau 10 giây sẽ gửi SMS)
                        triggerAlert(ALERT_LEVEL1, "Motion detected by PIR sensor");
                    }
                }
            } else {
                if (motionInProgress && (millis() - motionStartTime > motionCooldown)) {
                    Serial.println("[MOTION] Motion continues > 20s, playing audio again");
                    motionStartTime = millis();
                    
                    if (!isAudioPlaying()) {
                        playAudio(AUDIO_MOTION_DETECTED);
                    }
                    
                    // Tăng mức cảnh báo lên cấp độ 2 nếu chuyển động kéo dài
                    if (currentAlertLevel == ALERT_LEVEL1) {
                        triggerAlert(ALERT_LEVEL2, "Extended motion detected (>20s)");
                    }
                }
            }
        } else {
            if (radarState == HIGH) {
                Serial.println("[MOTION] Motion stopped");
                radarState = LOW;
                motionInProgress = false;
                
                // Reset alert level sau một thời gian nếu không còn chuyển động
                // (Không reset ngay để có thể xử lý các SMS đã gửi)
            }
        }
    }
}

void getSensorsStatus() {
    if (systemReady) {
        Serial.println("[SENSORS] === SENSOR STATUS ===");

        int currentReading = digitalRead(PIR_PIN);
        unsigned long timeSinceLastMotion = millis() - lastMotionTime;
        
        Serial.printf("[LD2410C] Status - OUT Pin: %d, State: %d, Ready: %s\n", 
                     currentReading, radarState, systemReady ? "YES" : "NO");
        Serial.printf("[LD2410C] Last motion: %lu ms ago, Motion in progress: %s\n", 
                     timeSinceLastMotion, motionInProgress ? "YES" : "NO");
        
        if (motionInProgress) {
            unsigned long currentMotionDuration = millis() - motionStartTime;
            Serial.printf("[LD2410C] Current motion duration: %lu ms\n", currentMotionDuration);
        }

        Serial.printf("[LDR] Current value: %d, Environment: %s, LED: %s\n", 
                     ldrValue, isDark ? "DARK" : "BRIGHT", ledState ? "ON" : "OFF");
        Serial.printf("[LDR] Thresholds - Dark: <%d, Bright: >%d\n", 
                     LDR_DARK_THRESHOLD, LDR_BRIGHT_THRESHOLD);
        Serial.printf("[LDR] Last read: %lu ms ago\n", millis() - lastLDRRead);
        
        Serial.println("[SENSORS] === END STATUS ===");
        
    } else {
        Serial.println("[SENSORS] Sensors not ready");
    }
}