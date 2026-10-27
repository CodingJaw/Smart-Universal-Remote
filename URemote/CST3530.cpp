#include "CST3530.h"

// Protocol and startup sequence matched to the working Waveshare V2
// implementation in CodingJaw/OMOTE-Firmware-WS2.8.
static constexpr uint8_t CST3530_ADDR = 0x58;
static constexpr uint32_t CST3530_DATA = 0xD0070000;
static constexpr uint32_t CST3530_ACK = 0xD00002AB;

CST3530::CST3530(int sda, int scl, int rst, int irq)
    : sda_(sda), scl_(scl), rst_(rst), irq_(irq) {}

bool CST3530::begin() {
  pressed_ = false;
  event = 1;
  gestureID = 0;
  Wire1.begin(sda_, scl_, 400000);
  Wire1.setTimeOut(50);

  pinMode(irq_, INPUT);
  pinMode(rst_, OUTPUT);
  // CST3530 V2 requires a longer reset and boot delay than CST328.
  digitalWrite(rst_, LOW);
  delay(100);
  digitalWrite(rst_, HIGH);
  delay(500);

  Wire1.beginTransmission(CST3530_ADDR);
  ready_ = (Wire1.endTransmission() == 0);
  Serial.printf("[CST3530] V2 touch %s at I2C 0x58 (SDA%d SCL%d)\n",
                ready_ ? "detected" : "NOT detected", sda_, scl_);
  return ready_;
}

bool CST3530::readRegister(uint32_t reg, uint8_t *data, uint8_t length) {
  Wire1.beginTransmission(CST3530_ADDR);
  for (int shift = 24; shift >= 0; shift -= 8)
    Wire1.write(static_cast<uint8_t>(reg >> shift));
  if (Wire1.endTransmission(false) != 0) return false;

  if (Wire1.requestFrom(CST3530_ADDR, static_cast<size_t>(length)) != length)
    return false;
  for (uint8_t i = 0; i < length; ++i) data[i] = Wire1.read();
  return true;
}

bool CST3530::acknowledge() {
  Wire1.beginTransmission(CST3530_ADDR);
  for (int shift = 24; shift >= 0; shift -= 8)
    Wire1.write(static_cast<uint8_t>(CST3530_ACK >> shift));
  return Wire1.endTransmission() == 0;
}

bool CST3530::available() {
  const uint32_t now = millis();
  if (!ready_) {
    // If power-up is delayed, reprobe without requiring another flash.
    if (now - lastPollMs_ < 2000U) return false;
    lastPollMs_ = now;
    Wire1.beginTransmission(CST3530_ADDR);
    ready_ = Wire1.endTransmission() == 0;
    if (ready_) Serial.println("[CST3530] controller detected on retry");
    return false;
  }
  // Matches the working V2 touch read interval.
  if (now - lastPollMs_ < 20U) return false;
  lastPollMs_ = now;
  ++readCount_;

  uint8_t frame[9] = {};
  const bool readOk = readRegister(CST3530_DATA, frame, sizeof(frame));
  const bool ackOk = acknowledge();
  const bool ok = readOk && ackOk;
  if (!ok) ++errorCount_;

  // Periodic diagnostic messages distinguish a dead bus from no gestures.
  if (now - lastReportMs_ >= 5000U) {
    Serial.printf("[CST3530] 5s reads=%lu errors=%lu irq=%d pressed=%u\n",
                  static_cast<unsigned long>(readCount_),
                  static_cast<unsigned long>(errorCount_),
                  digitalRead(irq_), pressed_ ? 1U : 0U);
    readCount_ = 0;
    errorCount_ = 0;
    lastReportMs_ = now;
  }

  bool active = false;
  uint16_t newX = 0, newY = 0;
  if (ok) {
    const uint8_t count = frame[3] & 0x0F;
    active = count > 0 && count <= 5 && (frame[8] & 0xF0) != 0;
    if (active) {
      newX = static_cast<uint16_t>(frame[4]) |
             (static_cast<uint16_t>(frame[7] & 0x0F) << 8);
      newY = static_cast<uint16_t>(frame[5]) |
             (static_cast<uint16_t>(frame[7] & 0xF0) << 4);
      active = newX < 240 && newY < 320;
    }
  }
  if (!active) {
    if (!pressed_) return false;
    // A single transient I2C failure must not prematurely end a swipe.
    if (!ok && now - lastValidTouchMs_ <= 80U) return false;
    pressed_ = false;
    event = 1;
    Serial.println("[CST3530] release");
    return true;
  }
  x = newX;
  y = newY;
  strength = frame[6];
  lastValidTouchMs_ = now;
  if (!pressed_) {
    pressed_ = true;
    event = 0;
    Serial.printf("[CST3530] down x=%u y=%u\n", x, y);
  } else {
    event = 2;
  }
  return true;
}
