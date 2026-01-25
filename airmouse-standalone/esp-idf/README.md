# ESP-IDF Air Mouse

BLE HID air mouse using ESP32-S3 and MPU6050 gyroscope.

## Hardware

- **MCU**: ESP32-S3-DevKitC-1
- **Sensor**: MPU6050 (I2C)
  - SDA: GPIO 4
  - SCL: GPIO 5

## Features

- BLE HID mouse (no drivers needed on host)
- Gyroscope-based motion control
- Automatic calibration on BLE connection
- Exponential moving average filtering
- Deadzone to prevent drift
- ~60Hz update rate

## Building

```bash
# Set up ESP-IDF environment
. $IDF_PATH/export.sh

# Configure (optional - defaults are set in sdkconfig.defaults)
idf.py set-target esp32s3

# Build
idf.py build

# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor
```

## Configuration

Tuning parameters in `main/main.c`:

| Parameter | Default | Description |
|-----------|---------|-------------|
| `ALPHA` | 0.3 | EMA filter (higher = less lag, more noise) |
| `DEADZONE` | 0.2 | Minimum movement threshold (deg/s) |
| `SENSITIVITY` | -1.0 | Movement multiplier |
| `UPDATE_INTERVAL_MS` | 16 | Update rate (~60Hz) |

I2C pins in `main/mpu6050.h`:

| Pin | GPIO |
|-----|------|
| SDA | 4 |
| SCL | 5 |

## Project Structure

```
esp-idf/
├── CMakeLists.txt          # Project CMake
├── sdkconfig.defaults      # Default configuration
├── README.md
└── main/
    ├── CMakeLists.txt      # Component CMake
    ├── main.c              # Application entry point
    ├── mpu6050.c/h         # MPU6050 I2C driver
    └── ble_hid_mouse.c/h   # BLE HID mouse service
```

## Usage

1. Power on the device
2. Search for "Air Mouse" in Bluetooth settings
3. Pair the device
4. Move the device to control the cursor

The gyroscope calibrates automatically when a BLE connection is established.
Keep the device still during calibration (about 1 second).
