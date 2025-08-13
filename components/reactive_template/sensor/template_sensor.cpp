#include "template_sensor.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome {
namespace reactive_template_ {

static const char *const TAG = "reactive.template.sensor";

void ReactiveTemplateSensor::setup() {
  this->disable_loop();

  if (!this->f_.has_value())
    return;

  for (uint8_t i = 0; i < this->dependsOnCount; i++) {
    this->dependsOnSensors[i]->add_on_state_callback([this](float state) {

      ESP_LOGVV(TAG, "Dependency sensor changed state to %f", state);

      // for (uint8_t i = 0; i < this->dependsOnCount; i++) {
      //   if (!this->dependsOnSensors[i]->has_state() || std::isnan(this->dependsOnSensors[i]->state)) {
      //     ESP_LOGE(TAG, "Dependency sensor '%s' is not ready", this->dependsOnSensors[i]->get_name().c_str());
      //     return;
      //   }
      // }

      // debounce call
      this->set_timeout("updateValue", 50, [this]() {
        auto val = (*this->f_)();
        if (val.has_value()) {
          this->publish_state(*val);
        }
      });
    });
  }
}
float ReactiveTemplateSensor::get_setup_priority() const { return setup_priority::HARDWARE; }
void ReactiveTemplateSensor::set_template(std::function<optional<float>()> &&f) { this->f_ = f; }
void ReactiveTemplateSensor::dump_config() {
  LOG_SENSOR("", "Reactive Template Sensor", this);
}

void ReactiveTemplateSensor::set_depends_on_sensors(sensor::Sensor * const *dependsOnSensors, uint8_t count) {
  this->dependsOnSensors = dependsOnSensors;
  this->dependsOnCount = count;
}

}  // namespace reactive_template_
}  // namespace esphome