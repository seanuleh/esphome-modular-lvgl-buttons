#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "mpu6050.h"
#include "ble_hid_mouse.h"

static const char *TAG = "airmouse";

// Performance tuning parameters
#define ALPHA           0.3f    // EMA filter coefficient (higher = less lag, more noise)
#define DEADZONE        0.2f    // Minimum movement threshold (deg/s)
#define SENSITIVITY     -1.0f   // Movement multiplier (negative inverts axis)
#define CALIBRATION_SAMPLES 150 // Number of samples for gyro calibration
#define UPDATE_INTERVAL_MS  16  // ~60Hz update rate

// Filtered gyro values
static float filtered_y = 0.0f;
static float filtered_z = 0.0f;

// Accumulated sub-pixel movement
static float acc_x = 0.0f;
static float acc_y = 0.0f;

// Calibration state
static bool calibrated = false;

static void airmouse_task(void *pvParameters)
{
    mpu6050_gyro_data_t gyro_data;
    int64_t last_update = 0;

    ESP_LOGI(TAG, "Air mouse task started");

    while (1) {
        // Only process when BLE is connected and MPU is ready
        if (ble_hid_mouse_is_connected() && mpu6050_is_ready()) {

            // Calibrate on first connection
            if (!calibrated) {
                ESP_LOGI(TAG, "BLE connected, calibrating gyroscope...");
                if (mpu6050_calibrate(CALIBRATION_SAMPLES) == ESP_OK) {
                    calibrated = true;
                    filtered_y = 0.0f;
                    filtered_z = 0.0f;
                    acc_x = 0.0f;
                    acc_y = 0.0f;
                }
            }

            // Rate limit to ~60Hz
            int64_t now = esp_timer_get_time() / 1000;
            if (now - last_update >= UPDATE_INTERVAL_MS) {
                last_update = now;

                // Read gyroscope data
                if (mpu6050_read_gyro(&gyro_data) == ESP_OK) {

                    // Apply Exponential Moving Average filter
                    filtered_y = (ALPHA * gyro_data.gyro_y) + ((1.0f - ALPHA) * filtered_y);
                    filtered_z = (ALPHA * gyro_data.gyro_z) + ((1.0f - ALPHA) * filtered_z);

                    // Apply deadzone
                    float out_y = (fabsf(filtered_y) > DEADZONE) ? filtered_y : 0.0f;
                    float out_z = (fabsf(filtered_z) > DEADZONE) ? filtered_z : 0.0f;

                    // Accumulate movement (gyro Z -> mouse X, gyro Y -> mouse Y)
                    acc_x += out_z * SENSITIVITY;
                    acc_y += out_y * SENSITIVITY;

                    // Extract integer pixel movement
                    int move_x = (int)acc_x;
                    int move_y = (int)acc_y;

                    // Send movement if non-zero
                    if (move_x != 0 || move_y != 0) {
                        // Clamp to int8_t range
                        if (move_x > 127) move_x = 127;
                        if (move_x < -127) move_x = -127;
                        if (move_y > 127) move_y = 127;
                        if (move_y < -127) move_y = -127;

                        ble_hid_mouse_move((int8_t)move_x, (int8_t)move_y);

                        // Keep fractional remainder
                        acc_x -= move_x;
                        acc_y -= move_y;
                    }
                }
            }
        } else {
            // Reset calibration when disconnected
            if (calibrated) {
                ESP_LOGI(TAG, "BLE disconnected, will recalibrate on reconnect");
                calibrated = false;
            }
        }

        // Small delay to yield CPU
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP-IDF Air Mouse ===");

    // Initialize MPU6050
    ESP_LOGI(TAG, "Initializing MPU6050...");
    esp_err_t ret = mpu6050_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize MPU6050: %s", esp_err_to_name(ret));
        ESP_LOGE(TAG, "Check I2C connections: SDA=GPIO%d, SCL=GPIO%d",
                 MPU6050_SDA_PIN, MPU6050_SCL_PIN);
        return;
    }

    // Initialize BLE HID Mouse
    ESP_LOGI(TAG, "Initializing BLE HID Mouse...");
    ret = ble_hid_mouse_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BLE HID Mouse: %s", esp_err_to_name(ret));
        return;
    }

    // Set battery level
    ble_hid_mouse_set_battery_level(90);

    // Create air mouse task
    xTaskCreate(airmouse_task, "airmouse", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Initialization complete. Waiting for BLE connection...");
}
