#include "security_system.h"
#include "wifi_manager.h"
#include "audio_handler.h"

// Biến trạng thái
SecurityState currentSecurityState = SECURITY_IDLE;
unsigned long motionDetectedTime = 0;
bool ownerSmsAlreadySent = false;
bool neighborSmsAlreadySent = false; 
bool familyMemberDetected = false;

bool mqttConnected = false;
HardwareSerial simSerial(1);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

void initSecuritySystem() {
    resetSecurityState();
    initSIM();
    
    if (wifiState == WIFI_STA_OK) {
        initMQTT();
    }
}

void initSIM() {
    simSerial.begin(115200, SERIAL_8N1, SIM_RX_PIN, SIM_TX_PIN);
    
    pinMode(SIM_POWER_PIN, OUTPUT);
    digitalWrite(SIM_POWER_PIN, HIGH);
    delay(3000);
    
    while(simSerial.available()) simSerial.read();
    
    if (sendCommand("AT", "OK", 2000)) {
        sendCommand("AT+CPIN?", "+CPIN: READY", 2000);
        sendCommand("AT+CMGF=1", "OK", 2000);
        sendCommand("AT+CSCS=\"GSM\"", "OK", 2000);
    }
}

bool sendCommand(const char* command, const char* expectedResponse, unsigned long timeout) {
    simSerial.println(command);
    
    unsigned long startTime = millis();
    String response = "";
    
    while (millis() - startTime < timeout) {
        if (simSerial.available()) {
            char c = simSerial.read();
            response += c;
            
            if (response.indexOf(expectedResponse) >= 0) {
                return true;
            }
        }
    }
    
    return false;
}

bool sendSMS(const char* phoneNumber, const char* message) {
    simSerial.print("AT+CMGS=\"");
    simSerial.print(phoneNumber);
    simSerial.println("\"");
    
    delay(500);
    
    unsigned long startTime = millis();
    bool gotPrompt = false;
    
    while (millis() - startTime < 5000 && !gotPrompt) {
        if (simSerial.available()) {
            char c = simSerial.read();
            if (c == '>') {
                gotPrompt = true;
                break;
            }
        }
    }
    
    if (!gotPrompt) {
        return false;
    }
    
    simSerial.print(message);
    delay(500);
    simSerial.write(26);
    
    startTime = millis();
    String response = "";
    bool success = false;
    
    while (millis() - startTime < 20000) {
        if (simSerial.available()) {
            char c = simSerial.read();
            response += c;
            
            if (response.indexOf("+CMGS:") >= 0 && response.indexOf("OK") >= 0) {
                success = true;
                break;
            }
            
            if (response.indexOf("ERROR") >= 0) {
                break;
            }
        }
    }
    
    return success;
}

void initMQTT() {
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    connectMQTT();
}

void connectMQTT() {
    if (mqttConnected || wifiState != WIFI_STA_OK) return;
    
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
        mqttConnected = true;
        
        mqttClient.subscribe(MQTT_TOPIC_COMMAND);
        mqttClient.subscribe(MQTT_TOPIC_FAMILY_DETECT);
        
        publishMQTTStatus("ESP32S3 camera online");
        
    } else {
        mqttConnected = false;
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (length == 0) return;
    
    char message[length + 1];
    for (unsigned int i = 0; i < length; i++) {
        message[i] = (char)payload[i];
    }
    message[length] = '\0';
    
    if (strcmp(topic, MQTT_TOPIC_FAMILY_DETECT) == 0) {
        StaticJsonDocument<300> doc;
        DeserializationError error = deserializeJson(doc, message);
        
        if (!error) {
            const char* user_name = doc["user_name"] | "Unknown";
            float confidence = doc["confidence"] | 0.0;
            const char* user_id = doc["user_id"] | "unknown";
            const char* event = doc["event"];
            
            bool is_family_detection = true;
            if (event != nullptr) {
                is_family_detection = (strcmp(event, "family_member_detected") == 0);
            }
            
            if (is_family_detection) {
                onFamilyMemberDetected();
                
                char confirmBuffer[256];
                StaticJsonDocument<200> confirmDoc;
                confirmDoc["status"] = "family_confirmed";
                confirmDoc["user_name"] = user_name;
                confirmDoc["timestamp"] = millis();
                serializeJson(confirmDoc, confirmBuffer);
                mqttClient.publish(MQTT_TOPIC_STATUS, confirmBuffer);
                
                return;
            }
        }
    }
}

void publishMQTTStatus(const char* message) {
    if (!mqttConnected) return;
    
    StaticJsonDocument<200> doc;
    doc["device"] = MQTT_CLIENT_ID;
    doc["status"] = message;
    doc["timestamp"] = millis();
    doc["security_state"] = currentSecurityState;
    
    char buffer[256];
    serializeJson(doc, buffer);
    
    mqttClient.publish(MQTT_TOPIC_STATUS, buffer);
}

void handleSecuritySystem() {
    static unsigned long lastMqttDebug = 0;
    if (millis() - lastMqttDebug > 30000) {
        lastMqttDebug = millis();
        Serial.printf("[MQTT] Connected: %s\n", mqttConnected ? "YES" : "NO");
    }
    
    if (!mqttConnected && wifiState == WIFI_STA_OK) {
        connectMQTT();
    }
    
    if (mqttConnected) {
        mqttClient.loop();
    }
    
    checkSecurityTimers();
}

void onMotionDetected() {
    Serial.println("[SECURITY] Motion detected - Starting security sequence");
    
    if (!isAudioPlaying()) {
        playAudio(AUDIO_MOTION_DETECTED);
    }
    
    currentSecurityState = SECURITY_MOTION_DETECTED;
    motionDetectedTime = millis();
    ownerSmsAlreadySent = false;
    neighborSmsAlreadySent = false;
    familyMemberDetected = false;
    
    currentSecurityState = SECURITY_WAITING_OWNER_SMS;
    
    if (mqttConnected) {
        StaticJsonDocument<200> doc;
        doc["event"] = "motion_detected";
        doc["timestamp"] = millis();
        doc["security_state"] = currentSecurityState;
        
        char buffer[256];
        serializeJson(doc, buffer);
        
        mqttClient.publish(MQTT_TOPIC_ALERT, buffer);
    }
    
    publishMQTTStatus("Motion detected - waiting for family confirmation");
}

void onFamilyMemberDetected() {
    Serial.println("[SECURITY] Family member detected - Resetting security state");
    
    familyMemberDetected = true;
    resetSecurityState();
    publishMQTTStatus("Family member confirmed - Security reset");
}

void resetSecurityState() {
    currentSecurityState = SECURITY_IDLE;
    motionDetectedTime = 0;
    ownerSmsAlreadySent = false;
    neighborSmsAlreadySent = false;
    familyMemberDetected = false;
    
    publishMQTTStatus("Security state reset to IDLE");
}

void checkSecurityTimers() {
    if (currentSecurityState == SECURITY_IDLE) {
        return;
    }
    
    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - motionDetectedTime;
    
    if (familyMemberDetected) {
        resetSecurityState();
        return;
    }
    
    switch (currentSecurityState) {
        case SECURITY_WAITING_OWNER_SMS:
            if (elapsedTime >= OWNER_SMS_DELAY && !ownerSmsAlreadySent) {
                if (sendSMS(PHONE_NUMBER_OWNER, "CANH BAO: Phat hien chuyen dong tai nha ban! Vui long kiem tra camera.")) {
                    Serial.println("[SECURITY] SMS sent to owner");
                    ownerSmsAlreadySent = true;
                } else {
                    Serial.println("[SECURITY] Failed to send SMS to owner");
                }
                
                currentSecurityState = SECURITY_WAITING_NEIGHBOR_SMS;
                publishMQTTStatus("Owner SMS sent");
            }
            break;
            
        case SECURITY_WAITING_NEIGHBOR_SMS:
            if (elapsedTime >= NEIGHBOR_SMS_DELAY && !neighborSmsAlreadySent) {
                if (sendSMS(PHONE_NUMBER_NEIGHBOR, "CANH BAO KHAN CAP: Co the co ke dot nhap tai nha hang xom! Vui long kiem tra giup.")) {
                    Serial.println("[SECURITY] SMS sent to neighbor");
                    neighborSmsAlreadySent = true;
                } else {
                    Serial.println("[SECURITY] Failed to send SMS to neighbor");
                }
                
                digitalWrite(LED_PIN, HIGH);
                currentSecurityState = SECURITY_ALARM_ACTIVE;
                publishMQTTStatus("Full alarm activated");
            }
            break;
            
        case SECURITY_ALARM_ACTIVE:
            if (elapsedTime >= 300000) {
                Serial.println("[SECURITY] Auto-reset after 5 minutes");
                resetSecurityState();
            }
            break;
            
        default:
            break;
    }
}