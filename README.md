# Modular easy button screen for ESPHome + LVGL on cheap touchscreen devices

## Supported Devices
* Guition `ESP32-4848s040` 4.0" with capacitive touch 120v/220v relays and build is power supply + USB-C [AliExpress Link](https://www.aliexpress.us/item/3256806436431838.html).
* Guition `JC3248W535` 3.5" with capacitive touch and USB-C [AliExpress Link](https://www.aliexpress.com/item/1005007566046827.html).
* Sunton `ESP32-8048S043` 4.3" with capactivive touch and USB-C [AliExpress Link](https://www.aliexpress.com/item/1005004788147691.html).
* Sunton `ESP32-8048S050` 5.0" with capactivive touch and USB-C [AliExpress Link](https://www.aliexpress.com/item/1005004952694042.html).
* Sunton `ESP32-8048s070` 7.0" with capactivive touch and USB-C [AliExpress Link](https://www.aliexpress.com/item/3256806438923653.html).
* Elecrow CrowPanel `DIS05035H` (v2.2) 3.5" with resistive touch and USB-C  [Manufacturer's Link](https://www.elecrow.com/esp32-display-3-5-inch-hmi-display-spi-tft-lcd-touch-screen.html).
* Waveshare `ESP32-S3-Touch-LCD-7` 7.0" with capactivive touch and USB-C [Manufacturer's Link](https://www.waveshare.com/esp32-s3-touch-lcd-7.htm).

# Install ESPHome on a build machine

I use a lot of svg vector graphics. The latest ESPHome does not install svg by default. Use pip to install cairosvg. 

```
pip install esphome cairosvg
```

### SDL Display on host

The SDL display platform allows you to use create an ESPHome display on a desktop system running Linux or MacOS. This is particularly useful for designing display layouts, since compiling and running a host binary is much faster than compiling for and flashing a microcontroller target system.

### `ESP32-4848s040` 4.0" 480px * 480px Smart Screen

This is a really great little screen. It has 120v/240v relays so it can control lights directly.  guition-esp32-s3-4848s040-display_modular.yaml has a boot screen, a system for dimming the backlight at night and some basic buttons for controlling local and Home Assistant devices.
