#include "coway_250s.h"
#include <sys/time.h>
#include <cassert>
#include <fstream>
#include <sstream>
#include <iostream>
int valid_frames=0;
static uint64_t fake_ms=0;
extern "C" int coway_test_gettimeofday(timeval *tv, void *) {
 tv->tv_sec=fake_ms/1000; tv->tv_usec=(fake_ms%1000)*1000; return 0;
}
struct Test : esphome::coway_250s::Coway250SComponent {
 void feed(const std::string &s) { for (auto c:s) process_byte_(c); }
 void reset_id() {last_command_id_=0;}
};
std::string unhex(std::string s) {
 std::string r; for(size_t i=0;i<s.size();i+=2) r+=char(std::stoul(s.substr(i,2),nullptr,16)); return r;
}
int main(int argc,char **argv) {
 Test c; esphome::sensor::Sensor power,light;
 c.set_power_sensor(&power); c.set_panel_lighting_sensor(&light);
 std::ifstream fixtures(argv[1]); std::string row;
 int reads=0,writes=0;
 while(std::getline(fixtures,row)) {
  std::istringstream in(row); char kind; std::string hex,attr; uint64_t stamp;
  in>>kind;
  if(kind=='R') {in>>hex; c.feed(unhex(hex)); ++reads;}
  else {in>>stamp>>attr>>hex; fake_ms=stamp; c.reset_id(); c.output.clear();
   if(attr=="{}") c.request_status();
   else c.send_control_attribute(attr.substr(1,4),std::stoi(attr.substr(6)));
   assert(c.output==unhex(hex)); ++writes;
  }
 }
 assert(valid_frames==reads);
 assert(power.has_state() && light.has_state());
 fake_ms=0; c.output.clear(); c.request_status(); assert(c.output.empty());
 fake_ms=1789648220120ULL; c.output.clear(); c.send_control_attribute("bad",1); assert(c.output.empty());
 c.feed("AT*ICT*AWS_SEND=\x02" "0021A10111789648220120-22{0007:1}26\x03\r");
 assert(valid_frames==reads); // Correct checksum but deliberately wrong length.
 c.feed("AT*ICT*AWS_SEND=\x02" "0020A10111789648220120-22{0007:1}00\x03\r");
 assert(valid_frames==reads); // Wrong checksum.
 std::cout<<reads<<" captured statuses accepted; "<<writes<<" authentic commands reproduced byte-for-byte; invalid time/key/length/checksum rejected\n";
}
