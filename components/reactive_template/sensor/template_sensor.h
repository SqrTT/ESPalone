#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace reactive_template_ {

class ReactiveTemplateSensor : public sensor::Sensor, public Component {
 public:
  void set_template(std::function<optional<float>()> &&f);

  void setup() override;

  void dump_config() override;

  float get_setup_priority() const override;

  void set_depends_on_sensors(sensor::Sensor * const *dependsOnSensors, uint8_t count);

 protected:
  sensor::Sensor * const *dependsOnSensors;
  optional<std::function<optional<float>()>> f_;
  uint8_t dependsOnCount;
};

}  // namespace reactive_template_
}  // namespace esphome