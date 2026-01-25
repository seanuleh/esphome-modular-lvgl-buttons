#include "mpu6050.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "mpu6050";

// MPU6050 Registers
#define REG_PWR_MGMT_1   0x6B
#define REG_GYRO_CONFIG  0x1B
#define REG_GYRO_XOUT_H  0x43

// Gyro sensitivity for 500 deg/s range
#define GYRO_SENSITIVITY 65.5f

static i2c_master_bus_handle_t bus_handle = NULL;
static i2c_master_dev_handle_t dev_handle = NULL;
static bool initialized = false;
static bool calibrated = false;
static float gyro_bias_y = 0.0f;
static float gyro_bias_z = 0.0f;

static esp_err_t mpu6050_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = {reg, value};
    return i2c_master_transmit(dev_handle, buf, sizeof(buf), 100);
}

static esp_err_t mpu6050_read_bytes(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(dev_handle, &reg, 1, data, len, 100);
}

esp_err_t mpu6050_init(void)
{
    esp_err_t ret;

    // Configure I2C bus
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = MPU6050_SDA_PIN,
        .scl_io_num = MPU6050_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ret = i2c_new_master_bus(&bus_config, &bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C bus: %s", esp_err_to_name(ret));
        return ret;
    }

    // Add MPU6050 device
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MPU6050_I2C_ADDR,
        .scl_speed_hz = MPU6050_I2C_FREQ_HZ,
    };

    ret = i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device: %s", esp_err_to_name(ret));
        return ret;
    }

    // Wake up MPU6050 (clear sleep bit)
    ret = mpu6050_write_reg(REG_PWR_MGMT_1, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to wake MPU6050: %s", esp_err_to_name(ret));
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    // Configure gyro range to 500 deg/s (FS_SEL = 1)
    ret = mpu6050_write_reg(REG_GYRO_CONFIG, 0x08);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure gyro: %s", esp_err_to_name(ret));
        return ret;
    }

    initialized = true;
    ESP_LOGI(TAG, "MPU6050 initialized successfully");
    return ESP_OK;
}

esp_err_t mpu6050_calibrate(int samples)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Calibrating gyroscope with %d samples...", samples);

    float sum_y = 0.0f;
    float sum_z = 0.0f;
    uint8_t buf[4];

    for (int i = 0; i < samples; i++) {
        // Read gyro Y and Z (skip X, start at GYRO_YOUT_H)
        esp_err_t ret = mpu6050_read_bytes(REG_GYRO_XOUT_H + 2, buf, 4);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Read error during calibration");
            continue;
        }

        int16_t raw_y = (int16_t)((buf[0] << 8) | buf[1]);
        int16_t raw_z = (int16_t)((buf[2] << 8) | buf[3]);

        sum_y += (float)raw_y / GYRO_SENSITIVITY;
        sum_z += (float)raw_z / GYRO_SENSITIVITY;

        vTaskDelay(pdMS_TO_TICKS(5));
    }

    gyro_bias_y = sum_y / samples;
    gyro_bias_z = sum_z / samples;
    calibrated = true;

    ESP_LOGI(TAG, "Calibration complete. Bias Y: %.3f, Z: %.3f", gyro_bias_y, gyro_bias_z);
    return ESP_OK;
}

esp_err_t mpu6050_read_gyro(mpu6050_gyro_data_t *data)
{
    if (!initialized || data == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t buf[4];
    esp_err_t ret = mpu6050_read_bytes(REG_GYRO_XOUT_H + 2, buf, 4);
    if (ret != ESP_OK) {
        return ret;
    }

    int16_t raw_y = (int16_t)((buf[0] << 8) | buf[1]);
    int16_t raw_z = (int16_t)((buf[2] << 8) | buf[3]);

    data->gyro_y = ((float)raw_y / GYRO_SENSITIVITY) - gyro_bias_y;
    data->gyro_z = ((float)raw_z / GYRO_SENSITIVITY) - gyro_bias_z;

    return ESP_OK;
}

bool mpu6050_is_ready(void)
{
    return initialized;
}

bool mpu6050_is_calibrated(void)
{
    return calibrated;
}
