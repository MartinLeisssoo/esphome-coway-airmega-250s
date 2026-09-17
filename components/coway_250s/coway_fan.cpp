#include "coway_fan.h"
#include <cstring>
#include "esphome/core/log.h"

namespace esphome::coway_250s {

static const char *const TAG = "coway_250s.fan";

void CowayFan::setup() {
  this->parent_->set_fan_state_callback([this](int power, int mode, int speed) {
    this->update_state_(power, mode, speed);
  });
}

fan::FanTraits CowayFan::get_traits() {
  fan::FanTraits traits(false, true, false, 3);
  this->wire_preset_modes_(traits);
  return traits;
}

void CowayFan::control(const fan::FanCall &call) {
  this->cancel_timeout("fan_start");
  this->pending_key_.clear();
  if (call.get_state().has_value() && !*call.get_state()) {
    this->parent_->send_control_attribute("0001", 0);
    return;
  }

  std::string key;
  int value = 0;
  if (call.get_speed().has_value()) {
    key = "0003";
    value = *call.get_speed();
  } else if (call.has_preset_mode()) {
    key = "0002";
    const char *preset = call.get_preset_mode();
    value = std::strcmp(preset, "Auto") == 0 ? 1 : std::strcmp(preset, "Sleep") == 0 ? 2 : 5;
  }

  if (!key.empty() && !this->state) {
    // Starting with a speed/preset requires power first. Wait for the MCU's
    // actual on-state before sending the second command; never publish guesses.
    this->pending_key_ = key;
    this->pending_value_ = value;
    this->set_timeout("fan_start", 3000, [this]() {
      this->pending_key_.clear();
      ESP_LOGW(TAG, "No power-on feedback; deferred fan setting cancelled");
    });
    this->parent_->send_control_attribute("0001", 1);
  } else if (!key.empty()) {
    this->parent_->send_control_attribute(key, value);
  } else if (call.get_state().has_value()) {
    this->parent_->send_control_attribute("0001", *call.get_state() ? 1 : 0);
  }
}

void CowayFan::update_state_(int power, int mode, int speed) {
  bool changed = !this->received_state_ || this->state != (power != 0);
  this->received_state_ = true;
  this->state = power != 0;
  if (this->state) {
    // Sleep/Rapid/Smart Eco are presets, not additional manual speed steps.
    const int manual_speed = speed >= 1 && speed <= 3 ? speed : 0;
    changed |= this->speed != manual_speed;
    this->speed = manual_speed;
    const char *preset = mode == 1 ? "Auto" : mode == 2 ? "Sleep" : mode == 5 ? "Rapid" : "";
    changed |= this->set_preset_mode_(preset);
  }
  // Keep the last speed/preset while off so a bare power-on resumes naturally.
  if (changed)
    this->publish_state();

  if (this->state && !this->pending_key_.empty()) {
    const std::string key = this->pending_key_;
    this->pending_key_.clear();
    this->cancel_timeout("fan_start");
    this->parent_->send_control_attribute(key, this->pending_value_);
  }
}

void CowayFan::dump_config() {
  LOG_FAN("", "Coway purifier", this);
}

}  // namespace esphome::coway_250s
