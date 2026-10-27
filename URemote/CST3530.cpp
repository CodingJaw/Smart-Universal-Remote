#include "CST3530.h"

// CST3530 reads a 4-byte register address at I2C 0x58. Unlike CST328,
// the read result is acknowledged with a separate 4-byte register write.
static constexpr uint8_t CST3530_ADDR = 0x58;
static constexpr uint32_t CST3530_DATA = 0xD0070000;
static constexpr uint32_t CST3530_ACK = 0xD00002AB;

CST3530::CST3530(int sda, int scl, int rst, int irq)
    : sda_(sda), scl_(scl), rst_(rst), irq_(irq) {}

bool CST3530::begin() {
  pressed_ = false;
  gestureID = 0;
  event = 1;

  pinMode(irq_, INPUT_PULLUP);
  pinMode(rst_, OUTPUT);
  digitalWrite(rst_, LOW);
  delay(10);
  digitalWrite(rst_, HIGH);
  delay(200);

  // The touch controller uses Wire1. Wire remains available for the IMU.
  if (!Wire1.begin(sda_, scl_, 400000)) {
    ets_printf("[CST3530] I2C bus setup failed\r\n");
    return false;
  }
  Wire1.beginTransmission(CST3530_ADDR);
  ready_ = (Wire1.endTransmission() == 0);
  ets_printf("[CST3530] Controller %s at 0x58\r\n",
             ready_ ? "found" : "NOT found");
  return ready_;
}

bool CST3530::readRegister(uint32_t reg, uint8_t *data, uint8_t length) {
  Wire1.beginTransmission(CST3530_ADDR);
  Wire1.write(static_cast<uint8_t>(reg >> 24));
  Wire1.write(static_cast<uint8_t>(reg >> 16));
  Wire1.write(static_cast<uint8_t>(reg >> 8));
  Wire1.write(static_cast<uint8_t>(reg));
  if (Wire1.endTransmission(false) != 0) return false;

  if (Wire1.requestFrom(CST3530_ADDR, length, true) != length) return false;
  for (uint8_t i = 0; i < length; ++i) data[i] = Wire1.read();
  return true;
}

bool CST3530::acknowledge() {
  Wire1.beginTransmission(CST3530_ADDR);
  Wire1.write(static_cast<uint8_t>(CST3530_ACK >> 24));
  Wire1.write(static_cast<uint8_t>(CST3530_ACK >> 16));
  Wire1.write(static_cast<uint8_t>(CST3530_ACK >> 8));
  Wire1.write(static_cast<uint8_t>(CST3530_ACK));
  return Wire1.endTransmission(true) == 0;
}

bool CST3530::available() {
  if (!ready_) return false;
  const uint32_t now = millis();
  if (now - lastPollMs_ < (pressed_ ? 8U : 16U)) return false;
  lastPollMs_ = now;

  // A single 9-byte report provides the number of contacts and the
  // first point's packed 12-bit coordinates.
  uint8_t report[9] = {};
  if (!readRegister(CST3530_DATA, report, sizeof(report))) {
    // Guard against remaining stuck pressed after a missing release report.
    if (!pressed_ || now - lastValidTouchMs_ <= 80U) return false;
    pressed_ = false;
    event = 1;
    return true;
  }
  acknowledge();

  const bool active = (report[3] & 0x0F) != 0 && (report[8] & 0xF0) != 0;
  if (!active) {
    if (!pressed_) return false;
    pressed_ = false;
    event = 1;
    return true;
  }

  const uint16_t rawX =
      (static_cast<uint16_t>(report[7] & 0x0F) << 8) | report[4];
  const uint16_t rawY =
      (static_cast<uint16_t>(report[7] & 0xF0) << 4) | report[5];
  if (rawX > 240 || rawY > 320) return false;

  x = rawX;
  y = rawY;
  strength = report[6];
  lastValidTouchMs_ = now;
  event = pressed_ ? 2 : 0;
  pressed_ = true;
  return true;
}
