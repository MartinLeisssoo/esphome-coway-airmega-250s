#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

namespace esphome {
namespace coway_250s {

class Coway250SComponent : public uart::UARTDevice, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  // A101 command grammar captured from the original paired Mercury module.
  void send_control_attribute(const std::string &key, int value);
  void request_status();
  void restart_handshake();
  void announce_connection();
  void send_diagnostic_sentence(const std::string &body);

  void set_fan_state_callback(std::function<void(int, int, int)> callback) {
    this->fan_state_callback_ = std::move(callback);
  }

  void set_power_sensor(sensor::Sensor *value) { this->power_sensor_ = value; }
  void set_operating_mode_sensor(sensor::Sensor *value) { this->operating_mode_sensor_ = value; }
  void set_fan_state_sensor(sensor::Sensor *value) { this->fan_state_sensor_ = value; }
  void set_panel_lighting_sensor(sensor::Sensor *value) { this->panel_lighting_sensor_ = value; }
  void set_off_timer_sensor(sensor::Sensor *value) { this->off_timer_sensor_ = value; }
  void set_sensitivity_sensor(sensor::Sensor *value) { this->sensitivity_sensor_ = value; }
  void set_button_lock_sensor(sensor::Sensor *value) { this->button_lock_sensor_ = value; }
  void set_pm25_sensor(sensor::Sensor *value) { this->pm25_sensor_ = value; }
  void set_pm10_sensor(sensor::Sensor *value) { this->pm10_sensor_ = value; }
  void set_ambient_light_sensor(sensor::Sensor *value) { this->ambient_light_sensor_ = value; }
  void set_prefilter_remaining_sensor(sensor::Sensor *value) { this->prefilter_remaining_sensor_ = value; }
  void set_max2_filter_remaining_sensor(sensor::Sensor *value) { this->max2_filter_remaining_sensor_ = value; }

 protected:
  static constexpr size_t MAX_LINE_LENGTH = 384;

  std::string line_buffer_;
  bool device_ready_sent_{false};
  bool connected_sent_{false};
  uint32_t setup_time_{0};
  uint32_t connect_due_{0};
  uint8_t connect_step_{0};
  uint64_t last_command_id_{0};
  std::function<void(int, int, int)> fan_state_callback_;
  sensor::Sensor *power_sensor_{nullptr};
  sensor::Sensor *operating_mode_sensor_{nullptr};
  sensor::Sensor *fan_state_sensor_{nullptr};
  sensor::Sensor *panel_lighting_sensor_{nullptr};
  sensor::Sensor *off_timer_sensor_{nullptr};
  sensor::Sensor *sensitivity_sensor_{nullptr};
  sensor::Sensor *button_lock_sensor_{nullptr};
  sensor::Sensor *pm25_sensor_{nullptr};
  sensor::Sensor *pm10_sensor_{nullptr};
  sensor::Sensor *ambient_light_sensor_{nullptr};
  sensor::Sensor *prefilter_remaining_sensor_{nullptr};
  sensor::Sensor *max2_filter_remaining_sensor_{nullptr};

  void process_byte_(uint8_t byte);
  void parse_sentence_(const std::string &sentence);
  void parse_status_frame_(const std::string &sentence);
  void publish_attributes_(const std::string &frame_type,
                           const std::unordered_map<std::string, int> &attributes);
  void publish_if_changed_(sensor::Sensor *target, float value);
  void write_line_(const std::string &body);
  void write_framed_command_(char operation, const std::string &attribute_block);
  void send_connected_sequence_();
};

}  // namespace coway_250s
}  // namespace esphome
