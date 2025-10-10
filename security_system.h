#ifndef SECURITY_SYSTEM_H
#define SECURITY_SYSTEM_H

#include "config.h"
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Cấu hình Module SIM và MQTT
#define SIM_TX_PIN          7     // ESP TX kết nối với RX của module SIM
#define SIM_RX_PIN          15    // ESP RX kết nối với TX của module SIM
#define SIM_POWER_PIN       6     // PEN của module SIM

#define MQTT_SERVER         "192.168.1.12"  // Địa chỉ IP của Raspberry Pi
#define MQTT_PORT           1883
#define MQTT_USER           "mqtt_user"
#define MQTT_PASSWORD       "mqtt_password"
#define MQTT_CLIENT_ID      "ESP32S3_SecurityCam"

// MQTT Topics
#define MQTT_TOPIC_COMMAND  "security/camera/command"
#define MQTT_TOPIC_STATUS   "security/camera/status"
#define MQTT_TOPIC_ALERT    "security/camera/alert"

// Số điện thoại liên hệ
#define PHONE_NUMBER_OWNER    "0976168240"   // Số chủ nhà
#define PHONE_NUMBER_NEIGHBOR "0976168240"   // Số hàng xóm (thay thế bằng số thật)

// Thời gian cảnh báo (ms)
#define ALERT_LEVEL1_DELAY  10000    // 10 giây cho cảnh báo cấp độ 1
#define ALERT_LEVEL2_DELAY  20000    // 20 giây cho cảnh báo cấp độ 2
#define ALARM_DURATION      30000    // 30 giây cho thời gian còi hú

// Audio indexes cho hệ thống bảo mật
#define AUDIO_INTRUDER_ALERT  4       // "Có người xâm nhập"

// Trạng thái cảnh báo
enum AlertLevel {
    ALERT_NONE = 0,         // Không có cảnh báo
    ALERT_FAMILIAR = 3,     // Người quen (sử dụng giá trị khác với NONE)
    ALERT_LEVEL1 = 1,       // Phát hiện người lạ khoảng 10s
    ALERT_LEVEL2 = 2        // Phát hiện người lạ khoảng 20s
};

extern AlertLevel currentAlertLevel;
extern unsigned long alertStartTime;
extern bool alarmActive;
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

// Xử lý cảnh báo
void handleSecuritySystem();
void triggerAlert(AlertLevel level, const char* message);
void stopAlarm();

// Hàm tiện ích cho module SIM
bool sendCommand(const char* command, const char* expectedResponse, unsigned long timeout);
bool sendSMS(const char* phoneNumber, const char* message);
void checkSimStatus();
void checkNetworkStatus();

#endif