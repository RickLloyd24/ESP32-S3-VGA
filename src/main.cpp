#include "Global.h"


void setup() {
  // Initialize serial for debug output
  Serial.begin(115200);
  delay(1000);  // Allow time for serial monitor to connect

  Serial.println("\n========================================");
  Serial.println("ESP32-S3 VGA + USB Keyboard");
  Serial.println("========================================\n");

  // Turn off the onboard RGB LED
  neopixelWrite(RGB_LED_PIN, 0, 0, 0);

  initVGA();
  update_display();
  initUSBKeyboard();
}

void loop() {
  char key = getKey();
  if (key) {
    Serial.printf("Key: '%c' (0x%02X)\n", key, key);
  }
  delay(10);
}
