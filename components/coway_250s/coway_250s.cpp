#include "coway_250s.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <sys/time.h>

#include "esphome/core/log.h"

namespace esphome {
namespace coway_250s {

static const char *const TAG = "coway_250s";
static const std::string RX_PREFIX = "AT*ICT*";
static const std::string TX_PREFIX = "*ICT*";

static bool is_hex_key(const std::string &key) {
  if (key.size() != 4)
    return false;
  return std::all_of(key.begin(), key.end(), [](unsigned char character) {
    return std::isxdigit(character) != 0;
  });
}

void Coway250SComponent::setup() {
  this->line_buffer_.reserve(MAX_LINE_LENGTH);
  this->setup_time_ = millis();
}

void Coway250SComponent::loop() {
  if (!this->device_ready_sent_ && millis() - this->setup_time_ >= 1000) {
    this->restart_handshake();
  }

  if (!this->connected_sent_ && this->connect_due_ != 0 &&
      static_cast<int32_t>(millis() - this->connect_due_) >= 0) {
    this->send_connected_sequence_();
  }

  while (this->available() > 0) {
    uint8_t byte;
    if (!this->read_byte(&byte))
      break;
    this->process_byte_(byte);
  }

}

void Coway250SComponent::process_byte_(uint8_t byte) {
  if (byte == '\n')
    return;

  if (byte == '\r') {
    if (!this->line_buffer_.empty()) {
      this->parse_sentence_(this->line_buffer_);
      this->line_buffer_.clear();
    }
    return;
  }

  if (this->line_buffer_.size() >= MAX_LINE_LENGTH - 1) {
    ESP_LOGW(TAG, "UART sentence exceeded %u bytes; discarding", MAX_LINE_LENGTH - 1);
    this->line_buffer_.clear();
    return;
  }
  this->line_buffer_.push_back(static_cast<char>(byte));
}

void Coway250SComponent::parse_sentence_(const std::string &sentence) {
  const size_t prefix = sentence.find(RX_PREFIX);
  if (prefix == std::string::npos) {
    ESP_LOGW(TAG, "Ignoring sentence without AT*ICT* prefix (%u bytes)", sentence.size());
    return;
  }

  // The saved boot decode starts with a NUL before AWS_SET. Resynchronize
  // rather than losing the first setup command to startup noise.
  if (prefix != 0)
    ESP_LOGW(TAG, "Discarding %u bytes before AT*ICT* prefix", static_cast<unsigned int>(prefix));
  const std::string body = sentence.substr(prefix + RX_PREFIX.size());
  ESP_LOGD(TAG, "UART RX: %s", body.c_str());
  if (body.rfind("AWS_SEND=", 0) == 0) {
    this->parse_status_frame_(body);
    // The original module separates acceptance from cloud-delivery completion.
    // Boot capture: ~30-40 ms to acceptance, then ~400 ms to SEND OK.
    this->set_timeout(35, [this]() { this->write_line_("AWS_SEND:OK"); });
    this->set_timeout(435, [this]() { this->write_line_("AWS_IND:SEND OK"); });
    return;
  }

  if (body.rfind("AWS_SET=", 0) == 0) {
    // A new main-board setup must be allowed to replay the connection sequence.
    this->connected_sent_ = false;
    this->connect_due_ = 0;
    this->connect_step_ = 0;
    ESP_LOGI(TAG, "Main board firmware announcement: %s", body.c_str());
    this->write_line_("AWS_SET:OK");
    return;
  }

  if (body.rfind("AUCONMODE=", 0) == 0) {
    this->write_line_("AUCONMODE:OK");
    return;
  }

  if (body.rfind("BLE_ADV=", 0) == 0) {
    // Match the original module observed in the boot capture. BLE setup is not
    // needed because network provisioning is handled by ESPHome.
    this->write_line_("BLE_ADV:ERROR 5");
    this->connect_step_ = 0;
    this->connect_due_ = millis() + 3770;
    return;
  }

  ESP_LOGW(TAG, "Unsupported main-board sentence: %s", body.c_str());
}

void Coway250SComponent::parse_status_frame_(const std::string &sentence) {
  const size_t equals = sentence.find('=');
  if (equals == std::string::npos || equals + 1 >= sentence.size() ||
      static_cast<uint8_t>(sentence[equals + 1]) != 0x02) {
    ESP_LOGW(TAG, "AWS_SEND did not contain an STX-framed payload");
    return;
  }

  const size_t stx = equals + 1;
  const size_t etx = sentence.find(static_cast<char>(0x03), stx + 1);
  if (etx == std::string::npos || etx < stx + 11) {
    ESP_LOGW(TAG, "Incomplete Coway status frame");
    return;
  }

  const std::string framed = sentence.substr(stx + 1, etx - stx - 1);
  const std::string data = framed.substr(0, framed.size() - 2);
  const std::string checksum_text = framed.substr(framed.size() - 2);

  char *checksum_end = nullptr;
  const unsigned long expected = std::strtoul(checksum_text.c_str(), &checksum_end, 16);
  uint8_t actual = 0x02;
  for (const unsigned char byte : data)
    actual = static_cast<uint8_t>(actual + byte);
  if (checksum_end == checksum_text.c_str() || *checksum_end != '\0' || actual != expected) {
    ESP_LOGW(TAG, "Coway checksum mismatch: got %s, calculated %02X", checksum_text.c_str(), actual);
    return;
  }

  if (data.size() < 8) {
    ESP_LOGW(TAG, "Coway frame too short");
    return;
  }
  const std::string length_text = data.substr(0, 4);
  char *length_end = nullptr;
  const unsigned long expected_length = std::strtoul(length_text.c_str(), &length_end, 16);
  if (!is_hex_key(length_text) || *length_end != '\0' || expected_length != data.size() - 1) {
    ESP_LOGW(TAG, "Coway frame length mismatch");
    return;
  }
  const std::string frame_type = data.substr(4, 4);
  const size_t open = data.find('{');
  const size_t close = data.rfind('}');
  if (open == std::string::npos || close == std::string::npos || close <= open) {
    ESP_LOGW(TAG, "Coway %s frame has no attribute block", frame_type.c_str());
    return;
  }

  std::unordered_map<std::string, int> attributes;
  size_t cursor = open + 1;
  while (cursor < close) {
    const size_t colon = data.find(':', cursor);
    if (colon == std::string::npos || colon >= close)
      break;
    const size_t comma = data.find(',', colon + 1);
    const size_t end = (comma == std::string::npos || comma > close) ? close : comma;
    const std::string key = data.substr(cursor, colon - cursor);
    const std::string value = data.substr(colon + 1, end - colon - 1);
    if (key.size() == 4 && !value.empty())
      attributes[key] = std::atoi(value.c_str());
    cursor = end + 1;
  }

  ESP_LOGD(TAG, "Valid %s frame with %u attributes", frame_type.c_str(), attributes.size());

  this->publish_attributes_(frame_type, attributes);
}

void Coway250SComponent::publish_attributes_(
    const std::string &frame_type, const std::unordered_map<std::string, int> &attributes) {
  const auto publish = [&](const char *key, sensor::Sensor *target, bool invert_percent = false) {
    const auto it = attributes.find(key);
    if (it == attributes.end())
      return;
    const float value = invert_percent ? std::max(0, std::min(100, 100 - it->second)) : it->second;
    this->publish_if_changed_(target, value);
  };

  if (frame_type == "A101") {
    if (this->fan_state_callback_ && attributes.count("0001") &&
        attributes.count("0002") && attributes.count("0003")) {
      this->fan_state_callback_(attributes.at("0001"), attributes.at("0002"), attributes.at("0003"));
    }
    publish("0001", this->power_sensor_);
    publish("0002", this->operating_mode_sensor_);
    publish("0003", this->fan_state_sensor_);
    publish("0007", this->panel_lighting_sensor_);
    publish("0008", this->off_timer_sensor_);
    publish("000A", this->sensitivity_sensor_);
    publish("0024", this->button_lock_sensor_);
  } else if (frame_type == "A102") {
    publish("0001", this->pm25_sensor_);
    publish("0002", this->pm10_sensor_);
    publish("0007", this->ambient_light_sensor_);
    publish("0011", this->prefilter_remaining_sensor_, true);
    publish("0012", this->max2_filter_remaining_sensor_, true);
  } else {
    ESP_LOGD(TAG, "No published entities for frame type %s", frame_type.c_str());
  }
}

void Coway250SComponent::publish_if_changed_(sensor::Sensor *target, float value) {
  if (target == nullptr)
    return;
  if (!target->has_state() || target->state != value)
    target->publish_state(value);
}

void Coway250SComponent::write_line_(const std::string &body) {
  ESP_LOGD(TAG, "UART TX: %s%s", TX_PREFIX.c_str(), body.c_str());
  this->write_str(TX_PREFIX.c_str());
  this->write_str(body.c_str());
  this->write_str("\r\n");
}

void Coway250SComponent::send_diagnostic_sentence(const std::string &body) {
  // Authenticated, one-sentence test path; never accept arbitrary setup/reset
  // commands or additional CR/LF-delimited sentences from a test payload.
  if (body.size() > 300 || body.find_first_of("\r\n") != std::string::npos ||
      body.find('\0') != std::string::npos ||
      (body.rfind("AWS_RECV:", 0) != 0 && body != "AWS_IND:DISCONNECTED" &&
       body != "AWS_IND:CONNECT OK" && body != "DEVICEREADY")) {
    ESP_LOGE(TAG, "Rejected diagnostic sentence");
    return;
  }
  this->write_line_(body);
}

void Coway250SComponent::send_control_attribute(const std::string &key, int value) {
  if (!is_hex_key(key)) {
    ESP_LOGE(TAG, "Refusing control command with invalid attribute key '%s'", key.c_str());
    return;
  }

  char attribute[32];
  const int written = snprintf(attribute, sizeof(attribute), "{%s:%d}", key.c_str(), value);
  if (written <= 0 || static_cast<size_t>(written) >= sizeof(attribute)) {
    ESP_LOGE(TAG, "Control attribute did not fit in the transmit buffer");
    return;
  }
  this->write_framed_command_('1', attribute);
}

void Coway250SComponent::request_status() {
  this->write_framed_command_('3', "{}");
}

void Coway250SComponent::write_framed_command_(char operation,
                                             const std::string &attribute_block) {
  // Authentic app frames use an operation digit, 13 decimal epoch-ms digits,
  // then -22. Replies echo the ID, with operation 0 (write) or 2 (query).
  timeval now{};
  gettimeofday(&now, nullptr);
  if (now.tv_sec < 1577836800) {
    ESP_LOGW(TAG, "Waiting for local Home Assistant time before sending commands");
    return;
  }
  const uint64_t timestamp = static_cast<uint64_t>(now.tv_sec) * 1000 + now.tv_usec / 1000;
  this->last_command_id_ = std::max(timestamp, this->last_command_id_ + 1);
  char message_id[15];
  snprintf(message_id, sizeof(message_id), "%c%013llu", operation,
           static_cast<unsigned long long>(this->last_command_id_));
  const std::string tail = std::string("A101") + message_id + "-22" + attribute_block;
  char length[5];
  snprintf(length, sizeof(length), "%04X", static_cast<unsigned int>(tail.size() + 3));
  const std::string data = std::string(length) + tail;

  uint8_t checksum = 0x02;
  for (const unsigned char byte : data)
    checksum = static_cast<uint8_t>(checksum + byte);
  char checksum_text[3];
  snprintf(checksum_text, sizeof(checksum_text), "%02X", checksum);

  ESP_LOGW(TAG, "A101 command TX: %s%s", data.c_str(), checksum_text);
  this->write_str(TX_PREFIX.c_str());
  this->write_str("AWS_RECV:");
  this->write_byte(0x02);
  this->write_str(data.c_str());
  this->write_str(checksum_text);
  this->write_byte(0x03);
  this->write_str("\r");
}

void Coway250SComponent::restart_handshake() {
  this->connected_sent_ = false;
  this->connect_due_ = 0;
  this->connect_step_ = 0;
  this->write_line_("DEVICEREADY");
  this->device_ready_sent_ = true;
  ESP_LOGI(TAG, "DEVICEREADY sent; watch for AWS_SET, AUCONMODE and BLE_ADV replies");
}

void Coway250SComponent::announce_connection() {
  this->connected_sent_ = false;
  this->connect_step_ = 0;
  this->connect_due_ = millis() + 1;
  ESP_LOGI(TAG, "Starting captured connection-announcement sequence");
}

void Coway250SComponent::send_connected_sequence_() {
  // Preserve the observed boot pacing instead of flooding the MCU with eight
  // back-to-back lines. Delays are measured from captures/01_boot.sr.
  // IPALLOCATED is compatibility text sent to the appliance, not ESP network
  // configuration. Use documentation addresses instead of a development LAN.
  static const char *const messages[] = {
      "ASSOCIATED:0",
      "IPALLOCATED:192.0.2.2 255.255.255.0 192.0.2.1 192.0.2.1",
      "AWS_IND:MQTT OK",
      "AWS_IND:SUBSCRIBE OK",
      "AWS_IND:SUBSCRIBE OK",
      "AWS_IND:SUBSCRIBE OK",
      "AWS_IND:SUBSCRIBE OK",
      "AWS_IND:CONNECT OK",
  };
  static constexpr uint32_t delays[] = {4000, 5210, 343, 348, 348, 335, 438};
  this->write_line_(messages[this->connect_step_]);
  if (this->connect_step_ == 7) {
    this->connected_sent_ = true;
    this->connect_due_ = 0;
    ESP_LOGI(TAG, "Connection announcements sent; watch for fresh A101/A102/A103");
  } else {
    this->connect_due_ = millis() + delays[this->connect_step_++];
  }
}

void Coway250SComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Coway Airmega 250S UART bridge:");
  ESP_LOGCONFIG(TAG, "  UART: 115200 8N1");
  ESP_LOGCONFIG(TAG, "  Build mode: captured A101 local control; original module must be disconnected");
  LOG_SENSOR("  ", "Power Raw", this->power_sensor_);
  LOG_SENSOR("  ", "Operating Mode Raw", this->operating_mode_sensor_);
  LOG_SENSOR("  ", "Fan State Raw", this->fan_state_sensor_);
  LOG_SENSOR("  ", "Panel Lighting Raw", this->panel_lighting_sensor_);
  LOG_SENSOR("  ", "Off Timer Raw", this->off_timer_sensor_);
  LOG_SENSOR("  ", "Sensitivity Raw", this->sensitivity_sensor_);
  LOG_SENSOR("  ", "Button Lock Raw", this->button_lock_sensor_);
  LOG_SENSOR("  ", "PM2.5", this->pm25_sensor_);
  LOG_SENSOR("  ", "PM10", this->pm10_sensor_);
  LOG_SENSOR("  ", "Ambient Light Raw", this->ambient_light_sensor_);
  LOG_SENSOR("  ", "Pre-filter Remaining", this->prefilter_remaining_sensor_);
  LOG_SENSOR("  ", "MAX2 Filter Remaining", this->max2_filter_remaining_sensor_);
}

}  // namespace coway_250s
}  // namespace esphome
