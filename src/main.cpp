#include "Global.h"


void setup() {
  // Initialize serial for debug output
  Serial.begin(115200);
  delay(1000);  // Allow time for serial monitor to connect
  
  Serial.println("\n========================================");
  Serial.println("ESP32-S3 VGA Test - Starting...");
  Serial.println("========================================\n");
  
  initVGA();
  VGAtest();
}

void loop() {
  // Optional: Run animated test to verify refresh
  // Uncomment the line below to enable frame counter
  // testVGAAnimated();
  
  delay(10);
}
