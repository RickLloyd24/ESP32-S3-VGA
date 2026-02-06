//============================================================
// Display.cpp - VGA Display Functions for ESP32-S3 (SpikePavel library)
#include "Global.h"
#include <FONT_8x8.h>
#include <FONT_9x16.h>
#include "FONT_8x16.h"

//============================================================
//                    PinConfig: r0,r1,r2,r3,r4, g0,g1,g2,g3,g4,g5,         b0,b1,b2,b3,b4, hSync, vSync
static const PinConfig pins(-1,-1,-1,-1,Red1Pin, -1,-1,-1,-1,-1,Green1Pin, -1,-1,-1,-1, Blue1Pin, HSyncPin, VSyncPin);
static Mode mode = Mode::MODE_320x240x60;
static VGA vga;

bool initVGA() {
	if (!vga.init(pins, mode, 8, 5)) return false;  // SCREEN=5 (unused) to avoid buggy clear()
	vga.start();
	vga.fillRect(0, 0, mode.hRes, mode.vRes, 0);  // Clear to black via fillRect
	return true;
}

void VGAtest() {
	int barWidth = mode.hRes / 8;
	int colors[8] = {
		vga.rgb(0,0,0),      // Black
		vga.rgb(255,0,0),    // Red
		vga.rgb(0,255,0),    // Green
		vga.rgb(0,0,255),    // Blue
		vga.rgb(255,255,0),  // Yellow
		vga.rgb(255,0,255),  // Magenta
		vga.rgb(0,255,255),  // Cyan
		vga.rgb(255,255,255) // White
	};
	for (int i = 0; i < 8; i++) {
		vga.fillRect(i * barWidth, 0, barWidth, mode.vRes, colors[i]);
	}
	vga.setFont(FONT_8x8);
	vga.setCursor(40, mode.vRes/2-8);
	vga.setTextColor(65535, 0);  // White on black (RGB565 values required for 8-bit text)
	vga.print("ESP32-S3 VGA 640x480 8-Color Test");
	vga.show();
}

void update_display() {
	vga.fillRect(0, 0, mode.hRes, mode.vRes, 0);  // Clear to black
	vga.setFont(FONT_8x16);
	vga.setCursor(40, 10);
	vga.setTextColor(63, 0);  // Yellow on black (8-bit direct: 0b00111111)
	vga.print("Rick's Beer Monitor");
	vga.show();
}

// Convert 64-color palette to RGB values (0-255)
void RGBFunction(int num, int &R, int &G, int &B) {
  if (num > 63) num = 63;
  R = (num / 16) * 85;           // 0, 85, 170, 255
  G = ((num % 16) / 4) * 85;     // 0, 85, 170, 255
  B = (num % 4) * 85;            // 0, 85, 170, 255
}