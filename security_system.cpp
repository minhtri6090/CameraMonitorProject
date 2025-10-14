#include "security_system.h"
#include "wifi_manager.h"
#include "audio_handler.h"

// Biến trạng thái mới
SecurityState currentSecurityState = SECURITY_IDLE;
unsigned long motionDetectedTime = 0;
bool ownerSmsAlreadySent = false;
bool neighborSmsAlreadySent = false; 
bool familyMemberDetected = false;

bool mqttConnected = false;
HardwareSerial simSerial(1);  // Sử dụng UART1 cho module SIM
WiFiClient espClient;
PubSubClient mqttClient(espClient);

void initSecuritySystem() {
    Serial.println("[SECURITY] Initializing security system...");
    
    // Reset trạng thái ban đầu
    resetSecurityState();
    
    // Khởi tạo Module SIM
    initSIM();
    
    // Khởi tạo MQTT
    if (wifiState == WIFI_STA_OK) {
        initMQTT();
    } else {
        Serial.println("[SECURITY] WiFi not connected, MQTT disabled");
    }
    
    Serial.println("[SECURITY] Security system initialized");
}

void initSIM() {
    Serial.println("[SIM] Initializing SIM module...");
    
    // Khởi tạo SIM Serial
    simSerial.begin(115200, SERIAL_8N1, SIM_RX_PIN, SIM_TX_PIN);
    
    // Cấu hình chân POWER
    pinMode(SIM_POWER_PIN, OUTPUT);
    digitalWrite(SIM_POWER_PIN, HIGH);
    delay(3000); // Chờ module khởi động
    
    // Xóa buffer
    while(simSerial.available()) simSerial.read();
    
    Serial.println("[SIM] Bắt đầu kiểm tra module...");
    
    // Kiểm tra kết nối
    if (sendCommand("AT", "OK", 2000)) {
        Serial.println("[SIM] Kết nối với module thành công");
        
        // Kiểm tra trạng thái SIM
        sendCommand("AT+CPIN?", "+CPIN: READY", 2000);
        
        // Lấy thông tin module
        sendCommand("ATI", "OK", 2000);
        
        // Kiểm tra cường độ tín hiệu
        sendCommand("AT+CSQ", "OK", 2000);
        
        // Kiểm tra trạng thái mạng
        sendCommand("AT+CPSI?", "OK", 2000);
        
        // Cài đặt chế độ SMS text
        sendCommand("AT+CMGF=1", "OK", 2000);
        
        // Cài đặt bảng mã
        sendCommand("AT+CSCS=\"GSM\"", "OK", 2000);
        
        // Gửi tin nhắn test
        Serial.println("[SIM] Gửi tin nhắn SMS test...");
        if (sendSMS(PHONE_NUMBER_OWNER, "He thong camera bao dong da khoi dong")) {
            Serial.println("[SIM] Test SMS sent successfully");
        } else {
            Serial.println("[SIM] Test SMS failed");
        }
    } else {
        Serial.println("[SIM] Không thể kết nối với module!");
    }
    
    Serial.println("[SIM] SIM module initialized");
}

bool sendCommand(const char* command, const char* expectedResponse, unsigned long timeout) {
    Serial.print(">> ");
    Serial.println(command);
    
    simSerial.println(command);
    
    unsigned long startTime = millis();
    String response = "";
    
    // Đọc phản hồi trong khoảng thời gian timeout
    while (millis() - startTime < timeout) {
        if (simSerial.available()) {
            char c = simSerial.read();
            response += c;
            Serial.write(c);
            
            // Nếu tìm thấy phản hồi mong đợi
            if (response.indexOf(expectedResponse) >= 0) {
                return true;
            }
        }
    }
    
    return false;
}

bool sendSMS(const char* phoneNumber, const char* message) {
    // Gửi lệnh AT+CMGS
    Serial.print(">> AT+CMGS=\"");
    Serial.print(phoneNumber);
    Serial.println("\"");
    
    simSerial.print("AT+CMGS=\"");
    simSerial.print(phoneNumber);
    simSerial.println("\"");
    
    delay(500);
    
    // Chờ dấu nhắc ">"
    unsigned long startTime = millis();
    bool gotPrompt = false;
    
    while (millis() - startTime < 5000 && !gotPrompt) {
        if (simSerial.available()) {
            char c = simSerial.read();
            Serial.write(c);
            
            if (c == '>') {
                gotPrompt = true;
                break;
            }
        }
    }
    
    if (!gotPrompt) {
        Serial.println("❌ Không nhận được dấu nhắc '>'");
        return false;
    }
    
    // Gửi nội dung tin nhắn
    Serial.print(">> ");
    Serial.println(message);
    
    simSerial.print(message);
    delay(500);
    
    // Gửi Ctrl+Z để kết thúc tin nhắn
    simSerial.write(26);
    
    // Đọc phản hồi
    startTime = millis();
    String response = "";
    bool success = false;
    
    while (millis() - startTime < 20000) {
        if (simSerial.available()) {
            char c = simSerial.read();
            response += c;
            Serial.write(c);
            
            // Kiểm tra phản hồi thành công
            if (response.indexOf("+CMGS:") >= 0 && response.indexOf("OK") >= 0) {
                success = true;
                break;
            }
            
            // Kiểm tra lỗi
            if (response.indexOf("ERROR") >= 0) {
                break;
            }
        }
    }
    
    return success;
}

void checkSimStatus() {
    Serial.println("[SIM] === SIM Status Check ===");
    sendCommand("AT+CPIN?", "OK", 2000);
    sendCommand("AT+CSQ", "OK", 2000);  // Signal quality
    sendCommand("AT+CREG?", "OK", 2000); // Registration status
    sendCommand("AT+CPSI?", "OK", 2000); // Network information
    Serial.println("[SIM] === End Status Check ===");
}

void checkNetworkStatus() {
    Serial.println("[SIM] Checking network status...");
    
    // Kiểm tra trạng thái SIM
    sendCommand("AT+CPIN?", "OK", 2000);
    
    // Kiểm tra cường độ tín hiệu
    sendCommand("AT+CSQ", "OK", 2000);
    
    // Kiểm tra trạng thái mạng
    sendCommand("AT+CPSI?", "OK", 2000);
    
    // Kiểm tra đăng ký mạng
    sendCommand("AT+CREG?", "OK", 2000);
    
    // Kiểm tra nhà mạng
    sendCommand("AT+COPS?", "OK", 2000);
}

void initMQTT() {
    Serial.println("[MQTT] Initializing MQTT...");
    
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    
    connectMQTT();
    
    Serial.println("[MQTT] MQTT initialized");
}

void connectMQTT() {
    if (mqttConnected || wifiState != WIFI_STA_OK) return;
    
    Serial.println("[MQTT] Connecting to MQTT broker...");
    Serial.printf("[MQTT] Broker: %s:%d\n", MQTT_SERVER, MQTT_PORT);
    Serial.printf("[MQTT] Client ID: %s\n", MQTT_CLIENT_ID);
    Serial.printf("[MQTT] Username: %s\n", MQTT_USER);
    
    // Kết nối MQTT broker
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
        Serial.println("[MQTT] ✅ Connected to MQTT broker successfully");
        mqttConnected = true;
        
        // Subscribe các topics với debug
        Serial.println("[MQTT] Subscribing to topics...");
        
        bool sub1 = mqttClient.subscribe(MQTT_TOPIC_COMMAND);
        Serial.printf("[MQTT] Subscribe to %s: %s\n", MQTT_TOPIC_COMMAND, sub1 ? "SUCCESS" : "FAILED");
        
        bool sub2 = mqttClient.subscribe(MQTT_TOPIC_FAMILY_DETECT);
        Serial.printf("[MQTT] Subscribe to %s: %s\n", MQTT_TOPIC_FAMILY_DETECT, sub2 ? "SUCCESS" : "FAILED");
        
        // Gửi thông báo online
        publishMQTTStatus("ESP32S3 camera online - MQTT connected");
        
    } else {
        Serial.print("[MQTT] ❌ Failed to connect, rc=");
        Serial.println(mqttClient.state());
        Serial.println("[MQTT] MQTT Error codes:");
        Serial.println("  -4: Connection timeout");
        Serial.println("  -3: Connection lost");
        Serial.println("  -2: Connect failed");
        Serial.println("  -1: Disconnected");
        Serial.println("   1: Bad protocol version");
        Serial.println("   2: Bad client ID");
        Serial.println("   3: Server unavailable");
        Serial.println("   4: Bad credentials");
        Serial.println("   5: Not authorized");
        mqttConnected = false;
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // Force print ngay lập tức
    Serial.flush();
    Serial.println();
    Serial.println("========================================");
    Serial.println("🚨 ESP32 MQTT CALLBACK TRIGGERED! 🚨");
    Serial.println("========================================");
    Serial.flush();
    
    // Print topic info
    Serial.print("📍 Topic: ");
    Serial.println(topic);
    Serial.print("📏 Length: ");
    Serial.println(length);
    Serial.flush();
    
    // Convert payload to string with safety check
    if (length == 0) {
        Serial.println("❌ Empty payload received!");
        return;
    }
    
    char message[length + 1];
    for (unsigned int i = 0; i < length; i++) {
        message[i] = (char)payload[i];
    }
    message[length] = '\0';
    
    Serial.print("💬 Message: ");
    Serial.println(message);
    Serial.flush();
    
    // Check if it's the family detection topic
    Serial.print("🔍 Comparing topic '");
    Serial.print(topic);
    Serial.print("' with '");
    Serial.print(MQTT_TOPIC_FAMILY_DETECT);
    Serial.println("'");
    
    int topicCompare = strcmp(topic, MQTT_TOPIC_FAMILY_DETECT);
    Serial.print("🔢 strcmp result: ");
    Serial.println(topicCompare);
    Serial.flush();
    
    if (topicCompare == 0) {
        Serial.println("✅ FAMILY DETECTION TOPIC MATCHED!");
        Serial.flush();
        
        // Parse JSON
        StaticJsonDocument<300> doc;
        DeserializationError error = deserializeJson(doc, message);
        
        if (!error) {
            const char* event = doc["event"];
            const char* user_name = doc["user_name"] | "Unknown";
            float confidence = doc["confidence"] | 0.0;
            
            Serial.print("🎯 Event: ");
            Serial.println(event ? event : "NULL");
            Serial.print("👤 User: ");
            Serial.println(user_name);
            Serial.print("📊 Confidence: ");
            Serial.println(confidence);
            Serial.flush();
            
            if (event && strcmp(event, "family_member_detected") == 0) {
                Serial.println("🎉🎉🎉 FAMILY MEMBER CONFIRMED! 🎉🎉🎉");
                Serial.printf("🏠 %s DETECTED (%.3f confidence)\n", user_name, confidence);
                Serial.printf("⚡ Current state before reset: %d\n", currentSecurityState);
                Serial.flush();
                
                // Reset security state
                onFamilyMemberDetected();
                
                Serial.println("✅ Security state has been reset!");
                Serial.flush();
                return;
            }
        } else {
            Serial.print("❌ JSON parse error: ");
            Serial.println(error.c_str());
            Serial.flush();
        }
    } else {
        Serial.println("ℹ️ Different topic - checking commands...");
        Serial.flush();
        
        if (strcmp(topic, MQTT_TOPIC_COMMAND) == 0) {
            Serial.println("🎮 Command topic matched");
            // Handle commands...
        }
    }
    
    Serial.println("========================================");
    Serial.flush();
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
    // Chỉ giữ MQTT debug - tắt tất cả debug khác
    static unsigned long lastMqttDebug = 0;
    if (millis() - lastMqttDebug > 10000) { // Tăng lên 10 giây
        lastMqttDebug = millis();
        Serial.printf("[MQTT] Connected: %s, Subscribed: %s, %s\n", 
                     mqttConnected ? "YES" : "NO", 
                     MQTT_TOPIC_FAMILY_DETECT, MQTT_TOPIC_COMMAND);
    }
    
    // Xử lý kết nối MQTT
    if (!mqttConnected && wifiState == WIFI_STA_OK) {
        connectMQTT();
    }
    
    // Xử lý MQTT messages với priority cao
    if (mqttConnected) {
        mqttClient.loop();
    }
    
    // Kiểm tra security timers
    checkSecurityTimers();
}

void onMotionDetected() {
    Serial.println("[SECURITY] Motion detected - Starting security sequence");
    
    // Phát âm thanh cảnh báo ngay lập tức
    if (!isAudioPlaying()) {
        playAudio(AUDIO_MOTION_DETECTED);
    }
    
    // Cập nhật trạng thái
    currentSecurityState = SECURITY_MOTION_DETECTED;
    motionDetectedTime = millis();
    ownerSmsAlreadySent = false;
    neighborSmsAlreadySent = false;
    familyMemberDetected = false;
    
    // Chuyển sang trạng thái chờ gửi SMS cho chủ nhà
    currentSecurityState = SECURITY_WAITING_OWNER_SMS;
    
    // Gửi thông báo qua MQTT
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
    
    // Reset ngay lập tức về trạng thái ban đầu
    resetSecurityState();
    
    // Gửi thông báo qua MQTT
    publishMQTTStatus("Family member confirmed - Security reset");
}

void resetSecurityState() {
    Serial.println("[SECURITY] Resetting security state to IDLE");
    
    currentSecurityState = SECURITY_IDLE;
    motionDetectedTime = 0;
    ownerSmsAlreadySent = false;
    neighborSmsAlreadySent = false;
    familyMemberDetected = false;
    
    // Tắt LED nếu đang bật
    digitalWrite(LED_PIN, LOW);
    
    publishMQTTStatus("Security state reset to IDLE");
}

void checkSecurityTimers() {
    if (currentSecurityState == SECURITY_IDLE) {
        return;
    }
    
    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - motionDetectedTime;
    
    // Hiển thị countdown timer
    static unsigned long lastCountdown = 0;
    if (millis() - lastCountdown > 1000) { // Mỗi giây
        lastCountdown = millis();
        
        if (currentSecurityState == SECURITY_WAITING_OWNER_SMS) {
            unsigned long remaining = OWNER_SMS_DELAY - elapsedTime;
            Serial.printf("[SECURITY] ⏰ Owner SMS in: %lu seconds\n", remaining / 1000);
        } else if (currentSecurityState == SECURITY_WAITING_NEIGHBOR_SMS) {
            unsigned long remaining = NEIGHBOR_SMS_DELAY - elapsedTime;
            Serial.printf("[SECURITY] ⏰ Neighbor SMS in: %lu seconds\n", remaining / 1000);
        }
    }
    
    // Kiểm tra family member detection TRƯỚC tất cả
    if (familyMemberDetected) {
        Serial.println("[SECURITY] 🏠 Family detected - IMMEDIATE RESET!");
        resetSecurityState();
        return;
    }
    
    switch (currentSecurityState) {
        case SECURITY_WAITING_OWNER_SMS:
            if (elapsedTime >= OWNER_SMS_DELAY && !ownerSmsAlreadySent) {
                Serial.println("[SECURITY] ⏰ 20 seconds elapsed - Sending SMS to owner");
                Serial.printf("[SECURITY] Family detected: %s\n", familyMemberDetected ? "YES" : "NO");
                
                if (sendSMS(PHONE_NUMBER_OWNER, "CANH BAO: Phat hien chuyen dong tai nha ban! Vui long kiem tra camera.")) {
                    Serial.println("[SECURITY] ✅ SMS sent to owner successfully");
                    ownerSmsAlreadySent = true;
                } else {
                    Serial.println("[SECURITY] ❌ Failed to send SMS to owner");
                }
                
                currentSecurityState = SECURITY_WAITING_NEIGHBOR_SMS;
                publishMQTTStatus("Owner SMS sent - waiting for neighbor SMS timer");
            }
            break;
            
        case SECURITY_WAITING_NEIGHBOR_SMS:
            if (elapsedTime >= NEIGHBOR_SMS_DELAY && !neighborSmsAlreadySent) {
                Serial.println("[SECURITY] ⏰ 40 seconds elapsed - Sending SMS to neighbor");
                
                if (sendSMS(PHONE_NUMBER_NEIGHBOR, "CANH BAO KHAN CAP: Co the co ke dot nhap tai nha hang xom! Vui long kiem tra giup.")) {
                    Serial.println("[SECURITY] ✅ SMS sent to neighbor successfully");
                    neighborSmsAlreadySent = true;
                } else {
                    Serial.println("[SECURITY] ❌ Failed to send SMS to neighbor");
                }
                
                digitalWrite(LED_PIN, HIGH);
                currentSecurityState = SECURITY_ALARM_ACTIVE;
                publishMQTTStatus("Full alarm activated - both SMS sent");
            }
            break;
            
        case SECURITY_ALARM_ACTIVE:
            if (elapsedTime >= 300000) { // 5 phút
                Serial.println("[SECURITY] Auto-reset after 5 minutes");
                resetSecurityState();
            }
            break;
            
        default:
            break;
    }
}