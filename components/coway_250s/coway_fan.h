#pragma once

#include "coway_250s.h"
#include "esphome/components/fan/fan.h"

namespace esphome::coway_250s {

class CowayFan : public Component, public fan::Fan {
 public:
  explicit CowayFan(Coway250SComponent *parent) : parent_(parent) {
    this->set_supported_preset_modes({"Auto", "Sleep", "Rapid"});
    // Avoid FanCall's default-to-maximum on the first bare turn-on call.
    this->speed = 1;
  }
  void setup() override;
  void dump_config() override;
  fan::FanTraits get_traits() override;

 protected:
  void control(const fan::FanCall &call) override;
  void update_state_(int power, int mode, int speed);

  Coway250SComponent *parent_;
  bool received_state_{false};
  std::string pending_key_;
  int pending_value_{0};
};

}  // namespace esphome::coway_250s
