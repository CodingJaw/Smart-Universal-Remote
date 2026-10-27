#pragma once
#include <Arduino.h>
#include <Wire.h>

// Waveshare ESP32-S3-Touch-LCD-2.8 V2 uses CST3530, not CST328.
// This interface matches the existing Display.cpp touch integration.
class CST3530 {
public:
  CST3530(int sda, int scl, int rst, int irq);
  bool begin();
  bool available();

  uint16_t x = 0;
  uint16_t y = 0;
  uint16_t strength = 0;
  int gestureID = 0;
  int event = 1; // 0: touch down, 1: release, 2: move/hold

private:
  bool readRegister(uint32_t reg, uint8_t *data, uint8_t length);
  bool acknowledge();
  int sda_;
  int scl_;
  int rst_;
  int irq_;
  bool ready_ = false;
  bool pressed_ = false;
  uint32_t lastPollMs_ = 0;
  uint32_t lastValidTouchMs_ = 0;
};
