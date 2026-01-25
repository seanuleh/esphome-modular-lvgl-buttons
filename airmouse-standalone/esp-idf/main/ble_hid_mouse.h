#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/**
 * Initialize BLE HID Mouse
 * @return ESP_OK on success
 */
esp_err_t ble_hid_mouse_init(void);

/**
 * Check if a host is connected
 * @return true if connected
 */
bool ble_hid_mouse_is_connected(void);

/**
 * Move the mouse cursor
 * @param x Relative X movement (-127 to 127)
 * @param y Relative Y movement (-127 to 127)
 * @return ESP_OK on success
 */
esp_err_t ble_hid_mouse_move(int8_t x, int8_t y);

/**
 * Move with wheel
 * @param x Relative X movement
 * @param y Relative Y movement
 * @param wheel Vertical wheel movement
 * @return ESP_OK on success
 */
esp_err_t ble_hid_mouse_move_wheel(int8_t x, int8_t y, int8_t wheel);

/**
 * Press mouse button(s)
 * @param buttons Button mask (bit 0: left, bit 1: right, bit 2: middle)
 * @return ESP_OK on success
 */
esp_err_t ble_hid_mouse_press(uint8_t buttons);

/**
 * Release all mouse buttons
 * @return ESP_OK on success
 */
esp_err_t ble_hid_mouse_release(void);

/**
 * Click a mouse button (press and release)
 * @param buttons Button mask
 * @return ESP_OK on success
 */
esp_err_t ble_hid_mouse_click(uint8_t buttons);

/**
 * Set battery level
 * @param level Battery level 0-100
 */
void ble_hid_mouse_set_battery_level(uint8_t level);

// Button masks
#define MOUSE_BUTTON_LEFT   (1 << 0)
#define MOUSE_BUTTON_RIGHT  (1 << 1)
#define MOUSE_BUTTON_MIDDLE (1 << 2)
