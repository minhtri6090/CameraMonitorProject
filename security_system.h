#ifndef SECURITY_SYSTEM_H
#define SECURITY_SYSTEM_H

#include "config.h"
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Cấu hình Module SIM và MQTT
#define SIM_TX_PIN          7
#define SIM_RX_PIN          15
#define SIM_POWER_PIN       6

#define MQTT_SERVER         "192.168.1.13"
#define MQTT_PORT           1883
#define MQTT_USER           "minhtri6090"
#define MQTT_PASSWORD       "123"
#define MQTT_CLIENT_ID      "ESP32S3_SecurityCam"

// MQTT Topics
#define MQTT_TOPIC_COMMAND       "security/camera/command"
#define MQTT_TOPIC_STATUS        "security/camera/status"
#define MQTT_TOPIC_ALERT         "security/camera/alert"
#define MQTT_TOPIC_FAMILY_DETECT "security/camera/family_detected"
#define MQTT_TOPIC_CONFIRMATION  "security/camera/confirmation"

// Số điện thoại
#define PHONE_NUMBER_OWNER    "0976168240"
#define PHONE_NUMBER_NEIGHBOR "0976168240"

// Thời gian cảnh báo
#define MOTION_DETECTION_TIMEOUT     20000
#define OWNER_SMS_DELAY              20000
#define NEIGHBOR_SMS_DELAY           40000

// Trạng thái hệ thống
enum SecurityState {
    SECURITY_IDLE = 0,
    SECURITY_MOTION_DETECTED,
    SECURITY_WAITING_OWNER_SMS,
    SECURITY_WAITING_NEIGHBOR_SMS,
    SECURITY_ALARM_ACTIVE
};

extern SecurityState currentSecurityState;
extern unsigned long motionDetectedTime;
extern bool ownerSmsAlreadySent;
extern bool neighborSmsAlreadySent;
extern bool familyMemberDetected;

extern bool mqttConnected;
extern PubSubClient mqttClient;
extern HardwareSerial simSerial;

// Khởi tạo
void initSecuritySystem();
void initSIM();
void initMQTT();

// MQTT
void connectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishMQTTStatus(const char* message);

// Xử lý bảo mật
void handleSecuritySystem();
void onMotionDetected();
void onFamilyMemberDetected();
void resetSecurityState();
void checkSecurityTimers();

// SIM
bool sendCommand(const char* command, const char* expectedResponse, unsigned long timeout);
bool sendSMS(const char* phoneNumber, const char* message);

#endif