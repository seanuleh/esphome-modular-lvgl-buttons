# ESP-NOW Battery Remote Project Progress

## STATUS: WORKING!

The delay-based script approach works. Remote wakes from deep sleep, waits 100ms for WiFi init, sends ESP-NOW, hub receives and fires IR.

## Project Goal
Build a battery-powered universal remote using ESP-NOW for ultra-low latency communication:
- **Hub (ESP32)**: Always-on, receives ESP-NOW, fires IR codes, integrated with Home Assistant
- **Remote (ESP32-C3)**: Battery-powered, wakes from deep sleep on button press, sends ESP-NOW, sleeps again

## Hardware
- Hub MAC: `B0:CB:D8:E3:2A:C4`
- Remote MAC: `A0:76:4E:76:B5:B8`
- Hub connected to WiFi on channel 10
- Remote uses CH340 USB adapter: `/dev/cu.wchusbserial552E0021791`

## Current Status: BLOCKING ISSUE
ESP-NOW send fails because WiFi isn't initialized when `on_boot` runs.

### The Problem
```
[D][main:109]: Woke up! Sending ESP-NOW...
[D][main:188]: ESP-NOW failed! Sleeping anyway...
[I][espnow:265]: Channel set to 10.   <-- WiFi starts AFTER send attempt
```

The espnow component's WiFi initialization happens at the END of its setup, but `on_boot` (at any priority) runs BEFORE WiFi is ready.

## Key Learnings

### 1. ESPHome espnow Standalone Mode WORKS
Without WiFi component, espnow initializes its own WiFi radio (see `espnow_component.cpp` lines 184-198):
```cpp
if (!this->is_wifi_enabled()) {
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    this->apply_wifi_channel();
}
```

### 2. ESP-IDF Framework Required
Arduino framework pulls in network libraries that cause linker errors without WiFi component. ESP-IDF works cleanly.

### 3. ESP32-C3 Deep Sleep GPIO Limitations
- Only GPIO 0-5 can wake from deep sleep
- GPIO9 (BOOT button) CANNOT wake the C3
- For final product, wire button to GPIO3

### 4. ESPHome on_boot Priority System
- Higher numbers = earlier execution during setup
- Negative numbers = later in setup
- BUT: All on_boot priorities run during setup phase
- espnow's WiFi init happens at END of its setup, after other components

### 5. Channel Synchronization is Critical
- ESP-NOW only works if both devices are on the same WiFi channel
- Hub is on channel 10 (via WiFi connection)
- Remote must set channel 10 explicitly

## Files

### ir-project.yaml (Hub) - WORKING
```yaml
wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
  ap:
    ssid: "ir-hub-ap"
    channel: 10

espnow:
  channel: 10
  auto_add_peer: true
  peers:
    - "A0:76:4E:76:B5:B8"
  on_receive:
    - if:
        condition:
          lambda: |-
            const uint8_t expected[6] = {0xA0, 0x76, 0x4E, 0x76, 0xB5, 0xB8};
            return memcmp(info.src_addr, expected, 6) == 0;
        then:
          - button.press: ac_power_button
```

### remote-test.yaml (Remote) - WORKING!
```yaml
esphome:
  name: "remote-test"
  on_boot:
    priority: -100
    then:
      - script.execute: send_and_sleep

esp32:
  board: esp32-c3-devkitm-1
  framework:
    type: esp-idf  # Required for standalone espnow

logger:
  level: DEBUG
  baud_rate: 115200
  hardware_uart: UART0

script:
  - id: send_and_sleep
    then:
      - delay: 100ms  # KEY: Wait for WiFi/ESP-NOW to initialize
      - logger.log: "Sending ESP-NOW..."
      - espnow.send:
          address: B0:CB:D8:E3:2A:C4
          data: "AC_POWER"
          on_sent:
            - logger.log: "ESP-NOW sent!"
            - delay: 50ms
            - deep_sleep.enter: deep_sleep_1
          on_error:
            - logger.log: "ESP-NOW failed!"
            - delay: 50ms
            - deep_sleep.enter: deep_sleep_1

espnow:
  channel: 10
  peers:
    - "B0:CB:D8:E3:2A:C4"

deep_sleep:
  id: deep_sleep_1
  sleep_duration: 5s
```

## Next Steps (Now That It Works)

1. **Add GPIO wakeup** - Wire button to GPIO 0-5 (e.g., GPIO3) for real button press wake
2. **Measure latency** - Time from wake to IR fire (currently ~150ms based on 100ms delay + send)
3. **Optimize delay** - Try reducing 100ms delay to find minimum reliable value
4. **Add more buttons** - Send different data strings for different IR codes
5. **Battery optimization** - Measure current draw, optimize sleep

## Previous Attempts (For Reference)

### Option 1: Use interval/script with delay
Instead of on_boot, use an interval that runs once after a short delay:
```yaml
script:
  - id: send_and_sleep
    then:
      - delay: 50ms  # Wait for WiFi to be ready
      - espnow.send:
          address: B0:CB:D8:E3:2A:C4
          data: "AC_POWER"
          on_sent:
            - deep_sleep.enter: deep_sleep_1
          on_error:
            - deep_sleep.enter: deep_sleep_1

esphome:
  on_boot:
    priority: -100
    then:
      - script.execute: send_and_sleep
```

### Option 2: Use on_loop with one-shot flag
```yaml
globals:
  - id: sent
    type: bool
    initial_value: 'false'

interval:
  - interval: 10ms
    then:
      - if:
          condition:
            lambda: 'return !id(sent);'
          then:
            - lambda: 'id(sent) = true;'
            - espnow.send: ...
```

### Option 3: Fork espnow component
Add a callback/event for "WiFi ready" that can trigger send action.

### Option 4: Pure ESP-IDF for remote
Write custom ESP-IDF code that properly sequences:
1. Initialize WiFi
2. Initialize ESP-NOW
3. Add peer
4. Send message
5. Wait for callback
6. Enter deep sleep

## Useful Commands
```bash
# Compile
esphome compile remote-test.yaml

# Flash
esphome upload remote-test.yaml --device /dev/cu.wchusbserial552E0021791

# Logs
esphome logs remote-test.yaml --device /dev/cu.wchusbserial552E0021791

# Hub logs (OTA)
esphome logs ir-project.yaml --device ir-project.local
```

## ESPHome Source Code Locations
- `/Users/sean/.asdf/installs/python/3.12.9/lib/python3.12/site-packages/esphome/components/espnow/__init__.py`
- `/Users/sean/.asdf/installs/python/3.12.9/lib/python3.12/site-packages/esphome/components/espnow/espnow_component.cpp`

Key lines in espnow_component.cpp:
- Lines 184-198: Standalone WiFi init
- Line 435: `peer_info.ifidx = WIFI_IF_STA;` (hardcoded interface)
- Lines 259-263: Channel blocked when WiFi enabled
