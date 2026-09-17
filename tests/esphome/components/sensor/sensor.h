#pragma once
namespace esphome::sensor {
class Sensor { public: float state=0; bool known=false; bool has_state() {return known;}
 void publish_state(float v) {state=v; known=true;} };
}
