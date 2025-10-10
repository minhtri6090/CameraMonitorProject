#include "security_system.h"
#include "wifi_manager.h"
#include "audio_handler.h"

AlertLevel currentAlertLevel = ALERT_NONE;
unsigned long alertStartTime = 0;
bool alarmActive = false;
bool mqttConnected = false;

HardwareSerial simSerial(1);  // Sử dụng UART1 cho module SIM
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Biến đếm thời gian và trạng thái
unsigned long alarmStartTime = 0;
bool smsSent = false;
bool neighborSmsSent = false;

void initSecuritySystem() {
    Serial.println("[SECURITY] Initializing security system...");
    
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
    
    // Khởi tạo SIM Serial (dùng UART1 như bạn đã test thành công)
    simSerial.begin(115200, SERIAL_8N1, SIM_RX_PIN, SIM_TX_PIN);
    
    // Cấu hình chân power và bật module
    pinMode(SIM_POWER_PIN, OUTPUT);
    digitalWrite(SIM_POWER_PIN, HIGH);
    
    // Chờ module khởi động
    delay(3000);
    
    // Kiểm tra kết nối
    simSerial.println("AT");
    delay(500);
    
    bool moduleOK = false;
    if (simSerial.available()) {
        String response = simSerial.readString();
        if (response.indexOf("OK") >= 0) {
            Serial.println("[SIM] Module responded OK");
            moduleOK = true;
        } else {
            Serial.println("[SIM] Module not responding correctly");
            Serial.println("[SIM] Response: " + response);
        }
    } else {
        Serial.println("[SIM] No response from SIM module");
    }
    
    // Cấu hình SMS mode
    if (moduleOK) {
        // Set SMS text mode
        simSerial.println("AT+CMGF=1");
        delay(500);
        
        // Set character set
        simSerial.println("AT+CSCS=\"GSM\"");
        delay(500);
        
        if (simSerial.available()) {
            String response = simSerial.readString();
            Serial.println("[SIM] Setup response: " + response);
        }
        
        // Gửi tin nhắn test
        if (sendSMS(PHONE_NUMBER_OWNER, "He thong camera bao dong da khoi dong")) {
            Serial.println("[SIM] Test SMS sent successfully");
        } else {
            Serial.println("[SIM] Test SMS failed");
        }
    }
    
    Serial.println("[SIM] SIM module initialized");
}

bool sendSMS(const char* phoneNumber, const char* message) {
    Serial.printf("[SIM] Sending SMS to %s: %s\n", phoneNumber, message);
    
    // Xóa buffer
    while (simSerial.available()) {
        simSerial.read();
    }
    
    // Đặt chế độ text
    simSerial.println("AT+CMGF=1");
    delay(500);
    
    // Đặt số điện thoại người nhận
    String cmd = "AT+CMGS=\"";
    cmd += phoneNumber;
    cmd += "\"";
    simSerial.println(cmd);
    
    // Đợi dấu nhắc ">"
    bool gotPrompt = false;
    unsigned long startTime = millis();
    String response = "";
    
    while (millis() - startTime < 5000) {
        if (simSerial.available()) {
            char c = simSerial.read();
            response += c;
            if (c == '>') {
                gotPrompt = true;
                break;
            }
        }
        delay(10);
    }
    
    if (!gotPrompt) {
        Serial.println("[SIM] No prompt received for SMS");
        return false;
    }
    
    // Gửi nội dung tin nhắn
    simSerial.print(message);
    delay(500);
    
    // Kết thúc tin nhắn với Ctrl+Z
    simSerial.write(26);
    delay(5000);  // Chờ gửi xong
    
    // Kiểm tra phản hồi
    response = "";
    startTime = millis();
    
    while (millis() - startTime < 10000) {  // Timeout 10 giây
        if (simSerial.available()) {
            while (simSerial.available()) {
                char c = simSerial.read();
                response += c;
            }
        }
        
        if (response.indexOf("+CMGS:") >= 0) {
            Serial.println("[SIM] SMS sent successfully");
            return true;
        } else if (response.indexOf("ERROR") >= 0) {
            Serial.println("[SIM] Failed to send SMS");
            Serial.println("[SIM] Error response: " + response);
            return false;
        }
        
        delay(100);
    }
    
    Serial.println("[SIM] SMS send timeout");
    return false;
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
    
    // Kết nối MQTT broker
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
        Serial.println("[MQTT] Connected to MQTT broker");
        mqttConnected = true;
        
        // Subscribe các topics
        mqttClient.subscribe(MQTT_TOPIC_COMMAND);
        
        // Gửi thông báo online
        publishMQTTStatus("ESP32S3 camera online");
    } else {
        Serial.print("[MQTT] Failed to connect, rc=");
        Serial.println(mqttClient.state());
        mqttConnected = false;
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // Chuyển payload thành string
    char message[length + 1];
    for (unsigned int i = 0; i < length; i++) {
        message[i] = (char)payload[i];
    }
    message[length] = '\0';
    
    Serial.print("[MQTT] Message received on topic: ");
    Serial.println(topic);
    Serial.print("[MQTT] Message: ");
    Serial.println(message);
    
    // Xử lý thông điệp từ MQTT
    if (strcmp(topic, MQTT_TOPIC_COMMAND) == 0) {
        // Parse JSON command
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, message);
        
        if (!error) {
            int alertLevel = doc["alert_level"];
            const char* alertMessage = doc["message"];
            
            // Cập nhật trạng thái cảnh báo
            triggerAlert((AlertLevel)alertLevel, alertMessage);
        }
    }
}

void publishMQTTStatus(const char* message) {
    if (!mqttConnected) return;
    
    StaticJsonDocument<200> doc;
    doc["device"] = MQTT_CLIENT_ID;
    doc["status"] = message;
    doc["timestamp"] = millis();
    
    char buffer[256];
    serializeJson(doc, buffer);
    
    mqttClient.publish(MQTT_TOPIC_STATUS, buffer);
}

void handleSecuritySystem() {
    // Xử lý kết nối MQTT
    if (!mqttConnected && wifiState == WIFI_STA_OK) {
        connectMQTT();
    }
    
    if (mqttConnected) {
        mqttClient.loop();
    }
    
    // Xử lý cảnh báo
    if (alarmActive) {
        unsigned long currentTime = millis();
        
        // Xử lý thời gian báo động
        if (currentAlertLevel == ALERT_LEVEL1) {
            // Cấp độ 1: Phát hiện người lạ > 10s
            if (!smsSent && (currentTime - alertStartTime > ALERT_LEVEL1_DELAY)) {
                Serial.println("[SECURITY] Alert level 1 timed threshold reached");
                sendSMS(PHONE_NUMBER_OWNER, "Canh bao: Co nguoi la xam nhap nha ban!");
                smsSent = true;
                
                // Phát âm thanh cảnh báo
                playAudio(AUDIO_INTRUDER_ALERT);
            }
        } 
        else if (currentAlertLevel == ALERT_LEVEL2) {
            // Cấp độ 2: Phát hiện người lạ > 20s
            if (!neighborSmsSent && (currentTime - alertStartTime > ALERT_LEVEL2_DELAY)) {
                Serial.println("[SECURITY] Alert level 2 timed threshold reached");
                
                // Kích hoạt còi báo động
                digitalWrite(LED_PIN, HIGH);  // Bật đèn
                
                // Gửi tin nhắn cho hàng xóm
                sendSMS(PHONE_NUMBER_NEIGHBOR, "Canh bao: Co ke trom xam nhap nha hang xom cua ban!");
                neighborSmsSent = true;
                
                // Phát âm thanh cảnh báo
                playAudio(AUDIO_INTRUDER_ALERT);
                
                // Thông báo trạng thái qua MQTT
                publishMQTTStatus("Alert level 2: Alarm activated!");
            }
        }
        
        // Tắt báo động sau một khoảng thời gian
        if (alarmStartTime > 0 && (currentTime - alarmStartTime > ALARM_DURATION)) {
            stopAlarm();
        }
    }
}

void triggerAlert(AlertLevel level, const char* message) {
    Serial.printf("[SECURITY] Alert triggered: Level %d - %s\n", level, message);
    
    // Cập nhật trạng thái cảnh báo hiện tại
    currentAlertLevel = level;
    alertStartTime = millis();
    
    // Xử lý theo từng cấp độ cảnh báo
    switch (level) {
        case ALERT_FAMILIAR: // Vì ALERT_FAMILIAR và ALERT_NONE có cùng giá trị 0, nên chỉ cần giữ 1 case
            Serial.println("[SECURITY] Familiar person detected");
            // Chỉ phát âm thanh cảnh báo, không làm gì thêm
            playAudio(AUDIO_INTRUDER_ALERT);
            break;
            
        case ALERT_LEVEL1:
            Serial.println("[SECURITY] Alert Level 1: Unfamiliar person detected");
            alarmActive = true;
            alarmStartTime = millis();
            smsSent = false;
            neighborSmsSent = false;
            break;
            
        case ALERT_LEVEL2:
            Serial.println("[SECURITY] Alert Level 2: Security threat detected");
            alarmActive = true;
            alarmStartTime = millis();
            
            // Nếu chưa gửi SMS ở level 1, gửi ngay
            if (!smsSent) {
                sendSMS(PHONE_NUMBER_OWNER, "CANH BAO KHAN: Co ke dot nhap nha ban!");
                smsSent = true;
            }
            break;
            
        default:
            // Tắt báo động nếu có
            stopAlarm();
            break;
    }
    
    // Cập nhật trạng thái lên MQTT
    if (mqttConnected) {
        char statusMsg[64];
        snprintf(statusMsg, sizeof(statusMsg), "Alert level changed to %d", level);
        publishMQTTStatus(statusMsg);
    }
}

void stopAlarm() {
    Serial.println("[SECURITY] Stopping alarm");
    
    // Tắt các thiết bị báo động
    digitalWrite(LED_PIN, LOW);  // Tắt đèn
    
    // Reset trạng thái
    alarmActive = false;
    alarmStartTime = 0;
    smsSent = false;
    neighborSmsSent = false;
    
    // Thông báo trạng thái
    publishMQTTStatus("Alarm stopped");
}

bool sendSMS(const char* phoneNumber, const char* message) {
    Serial.printf("[SIM] Sending SMS to %s: %s\n", phoneNumber, message);
    
    // Xóa buffer
    while (simSerial.available()) {
        simSerial.read();
    }
    
    // Đặt chế độ text
    simSerial.println("AT+CMGF=1");
    delay(500);
    
    // Đặt số điện thoại người nhận
    simSerial.print("AT+CMGS=\"");
    simSerial.print(phoneNumber);
    simSerial.println("\"");
    delay(500);
    
    // Gửi tin nhắn
    simSerial.print(message);
    delay(500);
    
    // Kết thúc tin nhắn với Ctrl+Z
    simSerial.write(26);
    delay(2000);  // Chờ gửi xong
    
    // Kiểm tra phản hồi
    String response = "";
    unsigned long startTime = millis();
    
    while (millis() - startTime < 10000) {  // Timeout 10 giây
        if (simSerial.available()) {
            response += simSerial.readString();
        }
        
        if (response.indexOf("OK") >= 0) {
            Serial.println("[SIM] SMS sent successfully");
            return true;
        } else if (response.indexOf("ERROR") >= 0) {
            Serial.println("[SIM] Failed to send SMS");
            Serial.println("[SIM] Error response: " + response);
            return false;
        }
        
        delay(100);
    }
    
    Serial.println("[SIM] SMS send timeout");
    return false;
}