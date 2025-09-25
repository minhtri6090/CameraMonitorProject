#ifndef SENSORS_HANDLER_H
#define SENSORS_HANDLER_H

#include "config.h"

// PIR/Motion sensor variables
extern bool systemReady;
extern bool motionDetected;
extern unsigned long lastMotionTime;
extern const unsigned long motionCooldown;
extern int radarState;
extern int radarVal;
extern unsigned long motionStartTime;
extern bool motionInProgress;

// LDR sensor variables
extern int ldrValue;
extern bool isDark;
extern bool ledState;
extern unsigned long lastLDRRead;

// Functions
void initializeSensors(); 
void getSensorsStatus(); 
void handleMotionLoop();
void handleLDRLoop();
void readLDRSensor();
void controlLED(bool turnOn);

void IRAM_ATTR radarISR();

#endif