#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

#ifdef USE_SENSOR
  #include "esphome/components/sensor/sensor.h"
#endif

namespace esphome {
namespace reactive_template_ {

class ReactiveTemplateBinarySensor : public Component, public binary_sensor::BinarySensor {
 public:
  void set_template(std::function<optional<bool>()> &&f) { this->f_ = f; }

  void setup() override;

  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::DATA; }

  void add_to_track(binary_sensor::BinarySensor *sensor_to_add);

  #ifdef USE_SENSOR
  void add_to_track(sensor::Sensor *sensor_to_add);
  #endif 

 protected:
  void execute();
  std::function<optional<bool>()> f_{nullptr};
};

}  // namespace template_
}  // namespace esphome
