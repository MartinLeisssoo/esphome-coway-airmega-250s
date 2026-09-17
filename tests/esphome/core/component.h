#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <string>
namespace esphome {
inline uint32_t millis() {return 0;}
class Component {public:
 virtual void setup() {} virtual void loop() {} virtual void dump_config() {}
 std::map<std::string,std::function<void()>> timers;
 void set_timeout(uint32_t,std::function<void()>) {}
 void set_timeout(const char *name,uint32_t,std::function<void()> f) {timers[name]=f;}
 void cancel_timeout(const char *name) {timers.erase(name);}
 void expire(const char *name) {auto f=timers.at(name); timers.erase(name); f();}
};
}
