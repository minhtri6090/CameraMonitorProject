#include "sensors_handler.h"
#include "audio_handler.h"
#include "security_system.h"

bool systemReady = false;
bool motionDetected = false;
unsigned long lastMotionTime = 0;
const unsigned long motionCooldown = 5000; // Giảm xuống 5s để phản hồi nhanh hơn

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
    // Chỉ điều khiển LED nếu không có báo động đang hoạt động
    if (currentSecurityState != SECURITY_ALARM_ACTIVE) {
        if (turnOn != ledState) {
            ledState = turnOn;
            digitalWrite(LED_PIN, ledState ? HIGH : LOW);
            Serial.printf("[LED] LED %s \n", ledState ? "ON" : "OFF");
        }
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
        
        if (radarVal == HIGH) {
            if (radarState == LOW) {
                if (millis() - lastMotionTime > motionCooldown) {
                    Serial.println("[MOTION] LD2410C Motion detected!");
                    lastMotionTime = millis();
                    motionStartTime = millis();
                    radarState = HIGH;
                    motionInProgress = true;

                    onMotionDetected();
                }
            }
        } else {
            if (radarState == HIGH) {
                radarState = LOW;
                motionInProgress = false;
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
        
        // Thêm thông tin security state
        Serial.printf("[SECURITY] Current state: %d\n", currentSecurityState);
        if (currentSecurityState != SECURITY_IDLE) {
            unsigned long elapsedTime = millis() - motionDetectedTime;
            Serial.printf("[SECURITY] Time since motion: %lu ms\n", elapsedTime);
            Serial.printf("[SECURITY] Owner SMS sent: %s, Neighbor SMS sent: %s\n", 
                         ownerSmsAlreadySent ? "YES" : "NO", 
                         neighborSmsAlreadySent ? "YES" : "NO");
        }
        
        Serial.println("[SENSORS] === END STATUS ===");
        
    } else {
        Serial.println("[SENSORS] Sensors not ready");
    }
}