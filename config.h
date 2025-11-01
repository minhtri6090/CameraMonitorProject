#ifndef CONFIG_H
#define CONFIG_H

// INCLUDES
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include "USB_STREAM.h"
#include "esp_heap_caps.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <SPI.h>
#include <SD.h>
#include "Audio.h"
#include <ESP32Servo.h>

#define FRAME_WIDTH 800
#define FRAME_HEIGHT 600
#define FRAME_INTERVAL 333333
#define MJPEG_BUF_SIZE (FRAME_WIDTH * FRAME_HEIGHT * 2)
#define USB_PAYLOAD_BUF_SIZE (32 * 1024)
#define USB_FRAME_BUF_SIZE (128 * 1024)

#define SD_CS     10
#define SPI_MOSI  12
#define SPI_MISO  13
#define SPI_SCK   11

#define I2S_DOUT  40 
#define I2S_BCLK  41
#define I2S_LRC   42

#define PIR_PIN   38

#define SERVO1_PIN 47
#define SERVO2_PIN 48

#define LDR_PIN 4              
#define LED_PIN 5      

// Cấu hình LDR
#define LDR_DARK_THRESHOLD 200 
#define LDR_BRIGHT_THRESHOLD 700 
#define LDR_READ_INTERVAL 1000  

#define AUDIO_HELLO                0  
#define AUDIO_WIFI_FAILED          1  
#define AUDIO_WIFI_SUCCESS         2  
#define AUDIO_MOTION_DETECTED      3 
#define AUDIO_ALARM_LEVEL1         4  // Index của file âm thanh cảnh báo cấp 1
#define AUDIO_ALARM_LEVEL2         5  // Index của file âm thanh cảnh báo cấp 2


#define MAX_CLIENTS 3
#define APP_CPU 1
#define PRO_CPU 0

#define V_SERVO1_LEFT   V10
#define V_SERVO1_RIGHT  V11
#define V_SERVO2_DOWN   V12
#define V_SERVO2_UP     V13
#define V_SERVO_CENTER  V14

enum WiFiState { 
    WIFI_STA_OK,   
    WIFI_AP_MODE    
};

#endif