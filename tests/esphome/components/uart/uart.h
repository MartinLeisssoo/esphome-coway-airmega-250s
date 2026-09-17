#pragma once
#include <string>
#include <cstdint>
namespace esphome::uart {
class UARTDevice { public: std::string output;
 int available() {return 0;} bool read_byte(uint8_t *) {return false;}
 void write_str(const char *s) {output+=s;} void write_byte(uint8_t b) {output+=char(b);}
};
}
