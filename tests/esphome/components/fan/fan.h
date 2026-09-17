#pragma once
#include <optional>
#include <string>
#include <vector>
namespace esphome::fan {
class FanTraits {public: FanTraits(bool,bool,bool,int n):count(n){} int count;};
class FanCall {
 public:
  std::optional<bool> state; std::optional<int> speed; std::string preset;
  auto get_state() const {return state;} auto get_speed() const {return speed;}
  bool has_preset_mode() const {return !preset.empty();}
  const char *get_preset_mode() const {return preset.c_str();}
};
class Fan {public:
 bool state=false; int speed=0; int publishes=0; std::string preset;
 virtual FanTraits get_traits()=0;
 void set_supported_preset_modes(std::initializer_list<const char *>) {}
 void publish_state() {++publishes;}
 protected:
 virtual void control(const FanCall &)=0;
 void wire_preset_modes_(FanTraits &) {}
 bool set_preset_mode_(const char *p) {bool change=preset!=p; preset=p; return change;}
};
}
#define LOG_FAN(...) ((void)0)
