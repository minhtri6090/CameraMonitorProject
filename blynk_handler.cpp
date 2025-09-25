#define BLYNK_TEMPLATE_ID "TMPL6Ulz28slZ"
#define BLYNK_TEMPLATE_NAME "ESP32S3_01"
#define BLYNK_AUTH_TOKEN "Nb1noOL0bTy4wNOnJ5XO5cF9DT_wICyr"

#include "blynk_handler.h"
#include "wifi_manager.h"
#include <BlynkSimpleEsp32.h>

Servo servo1, servo2;

int servo1Angle = 90;
int servo2Angle = 90;

volatile bool holdServo1Left = false;
volatile bool holdServo1Right = false;
volatile bool holdServo2Down = false;
volatile bool holdServo2Up = false;

const int MIN_ANGLE = 0;
const int MAX_ANGLE = 180;
const int SERVO_STEP = 4;
const int SERVO_SPEED_MS = 60;

BlynkTimer servoTimer;
unsigned long lastServoUpdate = 0;

void initializeBlynk() {
    if(savedSSID.length() > 0 && savedPassword.length() > 0) {
        Blynk.begin(BLYNK_AUTH_TOKEN, savedSSID.c_str(), savedPassword.c_str());
        Serial.println("[BLYNK] Đã khởi tạo Blynk connection");

        servoTimer.setInterval(SERVO_SPEED_MS, updateServoPositions);
        
    } else {
        Serial.println("[BLYNK] Không có WiFi credentials, bỏ qua Blynk");
    }
}

void initializeServos() {
    Serial.println("[SERVO] Initializing servos...");

    servo1.setPeriodHertz(50); 
    servo2.setPeriodHertz(50);

    servo1.attach(SERVO1_PIN, 500, 2400);
    servo2.attach(SERVO2_PIN, 500, 2400);

    servo1.write(servo1Angle);
    servo2.write(servo2Angle);
    
    delay(500);
    
    Serial.printf("[SERVO] Servos initialized - Pan: %d°, Tilt: %d°\n", servo1Angle, servo2Angle);
}

void updateServoPositions() {
    bool moved = false;

    if (holdServo1Left && servo1Angle > MIN_ANGLE) {
        servo1Angle = max(MIN_ANGLE, servo1Angle - SERVO_STEP);
        moved = true;
    }
    if (holdServo1Right && servo1Angle < MAX_ANGLE) {
        servo1Angle = min(MAX_ANGLE, servo1Angle + SERVO_STEP);
        moved = true;
    }

    if (holdServo2Down && servo2Angle > MIN_ANGLE) {
        servo2Angle = max(MIN_ANGLE, servo2Angle - SERVO_STEP);
        moved = true;
    }
    if (holdServo2Up && servo2Angle < MAX_ANGLE) {
        servo2Angle = min(MAX_ANGLE, servo2Angle + SERVO_STEP);
        moved = true;
    }

    if (moved) {
        servo1.write(servo1Angle);
        servo2.write(servo2Angle);    
        Serial.printf("[SERVO] Pan: %d°, Tilt: %d°\n", servo1Angle, servo2Angle);
    }
}

void moveServoToCenter() {
    servo1Angle = 90;
    servo2Angle = 90;
    
    servo1.write(servo1Angle);
    servo2.write(servo2Angle);
    

    
    Serial.println("[SERVO] Moved to center position (90°, 90°)");
}

void handleServoLoop() {
    if (Blynk.connected()) {
        servoTimer.run();
    }
}

void handleBlynkLoop() {
    if(wifiState == WIFI_STA_OK && WiFi.status() == WL_CONNECTED) {
        Blynk.run();
        handleServoLoop();
    }
}

BLYNK_WRITE(V_SERVO1_LEFT) {
    holdServo1Left = (param.asInt() == 1);
    Serial.printf("[BLYNK] Servo1 Left: %s\n", holdServo1Left ? "PRESSED" : "RELEASED");
}

BLYNK_WRITE(V_SERVO1_RIGHT) {
    holdServo1Right = (param.asInt() == 1);
    Serial.printf("[BLYNK] Servo1 Right: %s\n", holdServo1Right ? "PRESSED" : "RELEASED");
}

BLYNK_WRITE(V_SERVO2_DOWN) {
    holdServo2Down = (param.asInt() == 1);
    Serial.printf("[BLYNK] Servo2 Down: %s\n", holdServo2Down ? "PRESSED" : "RELEASED");
}

BLYNK_WRITE(V_SERVO2_UP) {
    holdServo2Up = (param.asInt() == 1);
    Serial.printf("[BLYNK] Servo2 Up: %s\n", holdServo2Up ? "PRESSED" : "RELEASED");
}

BLYNK_WRITE(V_SERVO_CENTER) {
    if (param.asInt() == 1) { 
        moveServoToCenter();
        Serial.println("[BLYNK] Center button pressed");
    }
}