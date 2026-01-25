#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

// The library needs NimBLE to be included before it if USE_NIMBLE is defined
#ifdef USE_NIMBLE
#include <NimBLEDevice.h>
#endif
#include <BleMouse.h>

namespace esphome {
namespace ble_mouse {

class BleMouseComponent : public Component {
 public:
  void setup() override {
    this->mouse_ = new BleMouse(this->device_name_, this->manufacturer_);
    this->mouse_->begin();
  }
  
  void loop() override {}
  
  void move(int8_t x, int8_t y, int8_t wheel, int8_t h_wheel) {
    if (this->mouse_->isConnected()) {
      this->mouse_->move(x, y, wheel, h_wheel);
    }
  }
  
  void click(uint8_t button) {
    if (this->mouse_->isConnected()) {
      this->mouse_->click(button);
    }
  }

  void set_device_name(const std::string &name) { this->device_name_ = name; }
  void set_manufacturer(const std::string &manufacturer) { this->manufacturer_ = manufacturer; }

 protected:
  BleMouse *mouse_;
  std::string device_name_;
  std::string manufacturer_;
};

template<typename... Ts> class MouseMoveAction : public Action<Ts...> {
 public:
  MouseMoveAction(BleMouseComponent *parent) : parent_(parent) {}
  
  void set_x(TemplatableValue<int8_t, Ts...> x) { x_ = x; }
  void set_y(TemplatableValue<int8_t, Ts...> y) { y_ = y; }
  void set_wheel(TemplatableValue<int8_t, Ts...> wheel) { wheel_ = wheel; }
  void set_h_wheel(TemplatableValue<int8_t, Ts...> h_wheel) { h_wheel_ = h_wheel; }

  void play(const Ts &...x) override {
    this->parent_->move(this->x_.value(x...), this->y_.value(x...), this->wheel_.value(x...), this->h_wheel_.value(x...));
  }

 protected:
  BleMouseComponent *parent_;
  TemplatableValue<int8_t, Ts...> x_{0};
  TemplatableValue<int8_t, Ts...> y_{0};
  TemplatableValue<int8_t, Ts...> wheel_{0};
  TemplatableValue<int8_t, Ts...> h_wheel_{0};
};

template<typename... Ts> class MouseClickAction : public Action<Ts...> {
 public:
  MouseClickAction(BleMouseComponent *parent) : parent_(parent) {}
  void set_button(TemplatableValue<uint8_t, Ts...> button) { button_ = button; }

  void play(const Ts &...x) override {
    this->parent_->click(this->button_.value(x...));
  }

 protected:
  BleMouseComponent *parent_;
  TemplatableValue<uint8_t, Ts...> button_{1};
};

}  // namespace ble_mouse
}  // namespace esphome
