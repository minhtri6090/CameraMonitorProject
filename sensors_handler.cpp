#include "sensors_handler.h"
#include "audio_handler.h"
#include "security_system.h"
#include "driver/gpio.h"

bool systemReady = false;
unsigned long lastMotionTime = 0;
const unsigned long motionCooldown = 5000;

int radarState = LOW; 
int radarVal = 0; 
unsigned long motionStartTime = 0;
bool motionInProgress = false;

// ✅ LDR & LED State
int ldrValue = 0;
bool isDark = false;
bool irLedState = false;
bool flashLedState = false;
unsigned long lastLDRRead = 0;

// ✅ Debounce cho motion update
unsigned long lastMotionUpdateTime = 0;
const unsigned long motionUpdateInterval = 500;

void initializeSensors() 
{
    pinMode(PIR_PIN, INPUT);
    
    pinMode(FLASH_LED_PIN, OUTPUT);
    digitalWrite(FLASH_LED_PIN, LOW);
    flashLedState = false;
    
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    irLedState = false;
    
    pinMode(LDR_PIN, INPUT);
    readLDRSensor();
    
    radarVal = digitalRead(PIR_PIN);
    radarState = LOW;
    motionInProgress = false;
    lastMotionTime = 0;
    lastMotionUpdateTime = 0;
    
    Serial.println("[PIR] Initialized (polling mode)");
    
    updateLEDsBasedOnConditions();
}

void readLDRSensor() {
    ldrValue = analogRead(LDR_PIN);

    bool wasDark = isDark;
    isDark = (ldrValue < LDR_DARK_THRESHOLD);
    
    if (wasDark != isDark) 
    {
        Serial.printf("[LDR] Light changed: %s (value=%d)\n", isDark ? "DARK" : "BRIGHT", ldrValue);
        updateLEDsBasedOnConditions();
    }
}

void controlIRLED(bool turnOn) 
{
    if (turnOn != irLedState) 
    {
        irLedState = turnOn;
        digitalWrite(LED_PIN, turnOn ? HIGH : LOW);
        Serial.printf("[IR_LED] %s\n", turnOn ? "ON" : "OFF");
    }
}

void controlFlashLED(bool turnOn) 
{
    if (turnOn != flashLedState) 
    {
        flashLedState = turnOn;
        digitalWrite(FLASH_LED_PIN, turnOn ? HIGH : LOW);
        Serial.printf("[FLASH_LED] %s\n", turnOn ? "ON" : "OFF");
    }
}

void updateLEDsBasedOnConditions() {
    if (isDark) 
    {
        if (motionInProgress) 
        {
            controlIRLED(false);
            controlFlashLED(true);
        } 
        else 
        {
            controlIRLED(true);
            controlFlashLED(false);
        }
    } 
    else 
    {
        controlIRLED(false);
        controlFlashLED(false);
    }
}

void handleLDRLoop() 
{
    if (systemReady && (millis() - lastLDRRead >= LDR_READ_INTERVAL)) 
    {
        lastLDRRead = millis();
        readLDRSensor();
    }
}

void resetMotionCooldown() 
{
    lastMotionTime = 0;
    Serial.println("[MOTION] Cooldown reset");
}

void handleMotionLoop() 
{
    if (!systemReady) return;
    
    radarVal = digitalRead(PIR_PIN);
    unsigned long currentTime = millis();
    
    // ✅ Motion START
    if (radarVal == HIGH && radarState == LOW) 
    {
        if (currentTime - lastMotionTime > motionCooldown) 
        {
            Serial.println("\n[MOTION] Motion started");
            
            lastMotionTime = currentTime;
            motionStartTime = currentTime;
            radarState = HIGH;
            motionInProgress = true;
            lastMotionUpdateTime = currentTime;
            
            updateLEDsBasedOnConditions();
            
            // ✅ Trigger security system (chỉ lần đầu)
            onMotionDetected();
            
        } 
        else 
        {
            radarState = HIGH;
        }
    }
    // ✅ Motion CONTINUE (throttle update)
    else if (radarVal == HIGH && radarState == HIGH) 
    {
        // ✅ Chỉ cập nhật mỗi 500ms
        if (currentTime - lastMotionUpdateTime >= motionUpdateInterval) 
        {
            lastMotionUpdateTime = currentTime;
            
            extern SecurityState currentSecurityState;
            if (currentSecurityState != SECURITY_IDLE) 
            {
                updateMotionTimestamp();
            }
        }
    }
    // ✅ Motion END → TẮT BUZZER NGAY LẬP TỨC
    else if (radarVal == LOW && radarState == HIGH) 
    {
        Serial.println("\n[MOTION] Motion ended");
        radarState = LOW;
        motionInProgress = false;
        
        updateLEDsBasedOnConditions();
        
        // ✅ GỌI HÀM TẮT BUZZER KHI MOTION END
        onMotionEnded();
    }
}