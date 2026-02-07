#ifndef GLOBAL_H
#define GLOBAL_H

//============ Includes ====================
#include <Arduino.h>
#include <VGA.h>                      //ESP32-S3-VGA library by SpikePavel

//============ ESP32-S3 Pin Definitions ===============
  // Mapped from Freenove Breakout Board physical positions
  #define Blue0Pin 40
  #define Blue1Pin 41
  #define Green0Pin 38
  #define Green1Pin 42
  #define Red0Pin 39
  #define Red1Pin 45                       
  #define HSyncPin 47                     
  #define VSyncPin 21
  #define RGB_LED_PIN 48                     
  #define KeyboardDplus 20              
  #define KeyboardDminus 19     

//============ VGA Display Function Declarations ===============
extern bool initVGA();                  // Initialize VGA at 640x480
extern void VGAtest();
extern void update_display();
extern void colorTest();

//============ USB Keyboard Function Declarations ===============
extern void initUSBKeyboard();
extern char getKey();

#endif