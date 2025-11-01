#include "sensors_handler.h"
#include "audio_handler.h"
#include "security_system.h"

bool systemReady = false;
bool motionDetected = false;
unsigned long lastMotionTime = 0;
const unsigned long motionCooldown = 5000;

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
    pinMode(PIR_PIN, INPUT);
    detachInterrupt(digitalPinToInterrupt(PIR_PIN));

    pinMode(LDR_PIN, INPUT);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    readLDRSensor();

    radarVal = digitalRead(PIR_PIN);
    radarState = LOW;
    motionDetected = false;
    motionInProgress = false;
    lastMotionTime = millis();
    
    if (radarVal == LOW) {
        attachInterrupt(digitalPinToInterrupt(PIR_PIN), radarISR, RISING);
    }
}

void readLDRSensor() {
    ldrValue = analogRead(LDR_PIN);

    Serial.printf("ADC: %d\n", ldrValue);
    if (!isDark && (ldrValue < LDR_DARK_THRESHOLD)) {
        isDark = true;
        digitalWrite(LED_PIN, HIGH);
        Serial.println("Led sang");
    } else if (isDark && ldrValue > LDR_BRIGHT_THRESHOLD) {
        isDark = false;
        digitalWrite(LED_PIN, LOW);
        Serial.println("Led tat");
    }
}

void controlLED(bool turnOn) {
    
    if (turnOn != ledState) {
        ledState = turnOn;
        digitalWrite(LED_PIN, ledState ? HIGH : LOW);
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
                    Serial.println("[MOTION] Motion detected");
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