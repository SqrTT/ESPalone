#include "template_sensor.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome {
namespace reactive_template_ {

static const char *const TAG = "reactive.template.sensor";

void ReactiveTemplateSensor::setup() {
  this->disable_loop();
}

float ReactiveTemplateSensor::get_setup_priority() const { return setup_priority::DATA; }
void ReactiveTemplateSensor::set_template(std::function<optional<float>()> &&f) { this->f_ = f; }
void ReactiveTemplateSensor::dump_config() {
  LOG_SENSOR("", "Reactive Template Sensor", this);
}

void ReactiveTemplateSensor::execute() {
  #ifdef ESPHOME_LOG_HAS_WARN
  if (this->f_ == nullptr) {
    ESP_LOGW(TAG, "No template function set for Reactive Template Sensor '%s'", this->get_name().c_str());
    return;
  }
  #endif
  // debounce call
  this->set_timeout("updateValue", 40, [this]() {
    auto val = (this->f_)();
    if (val.has_value()) {
      this->publish_state(*val);
    }
  });
}

void ReactiveTemplateSensor::add_to_track(sensor::Sensor *sensor_to_add) {
  sensor_to_add->add_on_state_callback([this](float state) {
    this->execute();
  });
}

#ifdef USE_BINARY_SENSOR
void ReactiveTemplateSensor::add_to_track(binary_sensor::BinarySensor *sensor_to_add) {
  sensor_to_add->add_on_state_callback([this](bool state) {
    this->execute();
  });
}
#endif


}  // namespace reactive_template_
}  // namespace esphome