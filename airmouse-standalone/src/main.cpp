#include <Arduino.h>
#include <BleMouse.h>
#include <Wire.h>

BleMouse bleMouse("Air Mouse", "Espressif", 100);

#define I2C_ADDR 0x68
#define SDA_PIN 4
#define SCL_PIN 5

#define REG_PWR_MGMT_1 0x6B
#define REG_GYRO_CONFIG 0x1B
#define REG_GYRO_XOUT_H 0x43

bool mpuFound = false;
float gyroBiasY = 0, gyroBiasZ = 0;
float filteredY = 0, filteredZ = 0;
bool calibrated = false;

// PERFORMANCE TUNING
const float alpha = 0.3;        // HIGHER = Less lag, but more noise (0.2 - 0.4)
const float deadzone = 0.2;     // Slightly lower for better small-movement response
const float sensitivity = -1.0; // Increased for better feel

float accX = 0, accY = 0; 
unsigned long lastUpdate = 0;

void wakeMPU() {
  Wire.beginTransmission(I2C_ADDR);
  Wire.write(REG_PWR_MGMT_1);
  Wire.write(0x00); 
  if (Wire.endTransmission() == 0) {
    mpuFound = true;
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(REG_GYRO_CONFIG);
    Wire.write(0x08); // 500 deg/s
    Wire.endTransmission();
  }
}

void calibrateMPU() {
  float sumY = 0, sumZ = 0;
  int samples = 150;
  for(int i = 0; i < samples; i++) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(REG_GYRO_XOUT_H + 2); 
    Wire.endTransmission(false);
    Wire.requestFrom(I2C_ADDR, 4);
    if (Wire.available() == 4) {
      sumY += (int16_t)((Wire.read() << 8) | Wire.read()) / 65.5;
      sumZ += (int16_t)((Wire.read() << 8) | Wire.read()) / 65.5;
    }
    delay(5);
  }
  gyroBiasY = sumY / samples;
  gyroBiasZ = sumZ / samples;
  calibrated = true;
  Serial.println("Calibrated.");
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Wire.begin(SDA_PIN, SCL_PIN, 400000); 
  wakeMPU();
  bleMouse.begin();
  bleMouse.setBatteryLevel(90);
}

void loop() {
  if (bleMouse.isConnected() && mpuFound) {
    if (!calibrated) calibrateMPU();

    unsigned long now = millis();
    // 16ms = ~60Hz update rate
    if (now - lastUpdate >= 16) {
      lastUpdate = now;
      
      Wire.beginTransmission(I2C_ADDR);
      Wire.write(REG_GYRO_XOUT_H + 2); 
      Wire.endTransmission(false);
      Wire.requestFrom(I2C_ADDR, 4);

      if (Wire.available() == 4) {
        int16_t rawY = (Wire.read() << 8) | Wire.read();
        int16_t rawZ = (Wire.read() << 8) | Wire.read();

        float currentY = (rawY / 65.5) - gyroBiasY;
        float currentZ = (rawZ / 65.5) - gyroBiasZ;

        // Exponential Moving Average
        filteredY = (alpha * currentY) + (1.0 - alpha) * filteredY;
        filteredZ = (alpha * currentZ) + (1.0 - alpha) * filteredZ;

        float outY = (abs(filteredY) > deadzone) ? filteredY : 0;
        float outZ = (abs(filteredZ) > deadzone) ? filteredZ : 0;

        accX += outZ * sensitivity;
        accY += outY * sensitivity;

        int moveX = (int)accX;
        int moveY = (int)accY;

        if (moveX != 0 || moveY != 0) {
          bleMouse.move(moveX, moveY);
          accX -= moveX;
          accY -= moveY;
        }
      }
    }
  } else {
    calibrated = false;
  }
  delay(1);
}
