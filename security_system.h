#ifndef SECURITY_SYSTEM_H
#define SECURITY_SYSTEM_H

#include "config.h"
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Cấu hình Module SIM và MQTT
#define SIM_TX_PIN          7     // ESP TX kết nối với RX của module SIM
#define SIM_RX_PIN          15    // ESP RX kết nối với TX của module SIM
#define SIM_POWER_PIN       6     // PEN của module SIM

#define MQTT_SERVER         "192.168.1.16"  // Địa chỉ IP của Raspberry Pi
#define MQTT_PORT           1883
#define MQTT_USER           "minhtri6090"
#define MQTT_PASSWORD       "123"
#define MQTT_CLIENT_ID      "ESP32S3_SecurityCam"

// MQTT Topics
#define MQTT_TOPIC_COMMAND       "security/camera/command"
#define MQTT_TOPIC_STATUS        "security/camera/status"
#define MQTT_TOPIC_ALERT         "security/camera/alert"
#define MQTT_TOPIC_FAMILY_DETECT "security/camera/family_detected"  // Topic nhận thông báo người nhà

// Số điện thoại liên hệ
#define PHONE_NUMBER_OWNER    "0976168240"   // Số chủ nhà
#define PHONE_NUMBER_NEIGHBOR "0976168240"   // Số hàng xóm (thay thế bằng số thật)

// Thời gian cảnh báo mới (ms)
#define MOTION_DETECTION_TIMEOUT     20000   // 20 giây cho mỗi giai đoạn
#define OWNER_SMS_DELAY              20000   // 20 giây đầu - gửi SMS cho chủ nhà
#define NEIGHBOR_SMS_DELAY           40000   // 40 giây tổng - gửi SMS cho hàng xóm

// Trạng thái hệ thống bảo mật mới
enum SecurityState {
    SECURITY_IDLE = 0,           // Không có chuyển động
    SECURITY_MOTION_DETECTED,    // Vừa phát hiện chuyển động
    SECURITY_WAITING_OWNER_SMS,  // Đang chờ 20s để gửi SMS chủ nhà
    SECURITY_WAITING_NEIGHBOR_SMS,// Đang chờ 20s tiếp để gửi SMS hàng xóm
    SECURITY_ALARM_ACTIVE        // Đã gửi SMS, báo động đang hoạt động
};

// Biến trạng thái mới
extern SecurityState currentSecurityState;
extern unsigned long motionDetectedTime;     // Thời điểm phát hiện chuyển động
extern bool ownerSmsAlreadySent;            // Đã gửi SMS cho chủ nhà chưa
extern bool neighborSmsAlreadySent;         // Đã gửi SMS cho hàng xóm chưa
extern bool familyMemberDetected;           // Đã nhận được thông báo người nhà chưa

extern bool mqttConnected;
extern PubSubClient mqttClient;
extern HardwareSerial simSerial;

// Khởi tạo
void initSecuritySystem();
void initSIM();
void initMQTT();

// Xử lý MQTT
void connectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishMQTTStatus(const char* message);

// Xử lý hệ thống bảo mật mới
void handleSecuritySystem();
void onMotionDetected();                    // Gọi khi phát hiện chuyển động
void onFamilyMemberDetected();              // Gọi khi nhận MQTT người nhà
void resetSecurityState();                  // Reset về trạng thái ban đầu
void checkSecurityTimers();                 // Kiểm tra thời gian và gửi SMS

// Hàm tiện ích cho module SIM
bool sendCommand(const char* command, const char* expectedResponse, unsigned long timeout);
bool sendSMS(const char* phoneNumber, const char* message);
void checkSimStatus();
void checkNetworkStatus();

#endif