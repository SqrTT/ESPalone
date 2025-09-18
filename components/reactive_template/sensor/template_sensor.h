#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#ifdef USE_BINARY_SENSOR
  #include "esphome/components/binary_sensor/binary_sensor.h"
#endif

namespace esphome {
namespace reactive_template_ {

class ReactiveTemplateSensor : public sensor::Sensor, public Component {
 public:
  void set_template(std::function<optional<float>()> &&f);

  void setup() override;

  void dump_config() override;

  float get_setup_priority() const override;

  void add_to_track(sensor::Sensor *sensor_to_add );
  #ifdef USE_BINARY_SENSOR
  void add_to_track(binary_sensor::BinarySensor *sensor_to_add);
  #endif

 protected:
  void execute();
  std::function<optional<float>()> f_{nullptr};
  uint8_t dependsOnCount;
};

}  // namespace reactive_template_
}  // namespace esphome