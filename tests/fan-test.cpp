#include "coway_fan.h"
#include <sys/time.h>
#include <cassert>
#include <cstdio>
#include <iostream>
int valid_frames=0;
extern "C" int coway_test_gettimeofday(timeval *tv,void *) {tv->tv_sec=1789650000;tv->tv_usec=0;return 0;}
struct Bridge : esphome::coway_250s::Coway250SComponent {
 void status(int power,int mode,int speed) {
  char attrs[64];std::snprintf(attrs,sizeof(attrs),"{0001:%d,0002:%d,0003:%d}",power,mode,speed);
  std::string tail=std::string("A10100000000000001-01")+attrs;
  char length[5];std::snprintf(length,sizeof(length),"%04X",unsigned(tail.size()+3));
  std::string data=std::string(length)+tail; unsigned sum=2;
  for(unsigned char c:data)sum+=c;
  char check[3];std::snprintf(check,sizeof(check),"%02X",sum&255);
  std::string sentence=std::string("AT*ICT*AWS_SEND=\x02")+data+check+"\x03\r";
  for(unsigned char c:sentence)process_byte_(c);
 }
};
struct Fan : esphome::coway_250s::CowayFan {
 using CowayFan::CowayFan; using CowayFan::control;
};
bool command(const Bridge &b,const char *s) {return b.output.find(s)!=std::string::npos;}
int main() {
 Bridge b;Fan f(&b);f.setup();assert(b.output.empty());assert(f.get_traits().count==3);
 b.status(1,0,1);assert(f.state&&f.speed==1&&f.preset.empty());
 auto n=f.publishes;b.status(1,0,1);assert(f.publishes==n);
 esphome::fan::FanCall off;off.state=false;f.control(off);
 assert(command(b,"{0001:0}"));assert(f.state); // No optimistic change.
 b.status(0,4,99);assert(!f.state);b.output.clear();
 esphome::fan::FanCall speed;speed.speed=2;speed.state=true;f.control(speed);
 assert(command(b,"{0001:1}"));assert(!command(b,"{0003:2}"));assert(!f.state);
 b.status(1,0,1);assert(command(b,"{0003:2}"));assert(f.speed==1);
 b.status(1,0,2);assert(f.speed==2&&f.state);
 for(auto p:{std::pair{"Auto",1},std::pair{"Sleep",2},std::pair{"Rapid",5}}) {
  b.output.clear();esphome::fan::FanCall c;c.preset=p.first;f.control(c);
  std::string expected="{0002:"+std::to_string(p.second)+"}";assert(command(b,expected.c_str()));
  b.status(1,p.second,p.second==1?2:p.second==2?0:5);assert(f.preset==p.first);
  assert(f.speed==(p.second==1?2:0));
 }
 speed.speed=3;f.control(speed);b.status(1,0,3);assert(f.speed==3&&f.preset.empty());
 b.status(0,4,99);f.control(speed);f.control(off);b.output.clear();b.status(1,0,1);
 assert(b.output.empty()); // Off cancels deferred speed.
 b.status(0,4,99);f.control(speed);f.expire("fan_start");b.output.clear();b.status(1,0,1);
 assert(b.output.empty()); // Timeout prevents stale deferred commands.
 std::cout<<"PASS: native 3-speed fan, presets, feedback-only state, off-to-on sequencing, cancellation and timeout\n";
}
