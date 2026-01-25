#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define MPU6050_I2C_ADDR    0x68
#define MPU6050_SDA_PIN     4
#define MPU6050_SCL_PIN     5
#define MPU6050_I2C_FREQ_HZ 400000

typedef struct {
    float gyro_y;
    float gyro_z;
} mpu6050_gyro_data_t;

/**
 * Initialize the MPU6050 sensor
 * @return ESP_OK on success
 */
esp_err_t mpu6050_init(void);

/**
 * Calibrate the gyroscope by sampling at rest
 * @param samples Number of samples to average (recommended: 150)
 * @return ESP_OK on success
 */
esp_err_t mpu6050_calibrate(int samples);

/**
 * Read calibrated gyroscope data
 * @param data Pointer to store gyro data (deg/s, bias-corrected)
 * @return ESP_OK on success
 */
esp_err_t mpu6050_read_gyro(mpu6050_gyro_data_t *data);

/**
 * Check if MPU6050 is initialized
 */
bool mpu6050_is_ready(void);

/**
 * Check if calibration is complete
 */
bool mpu6050_is_calibrated(void);
