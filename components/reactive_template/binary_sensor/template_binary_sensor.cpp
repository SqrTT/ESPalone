#include "template_binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace reactive_template_ {

static const char *const TAG = "reactive.template.binary_sensor";

void ReactiveTemplateBinarySensor::setup() {
  this->disable_loop();
}


#ifdef USE_SENSOR
void ReactiveTemplateBinarySensor::add_to_track(sensor::Sensor *sensor_to_add) {
  sensor_to_add->add_on_state_callback([this](float state) {
    this->execute();
  });
}
#endif

void ReactiveTemplateBinarySensor::execute() {
  #ifdef ESPHOME_LOG_HAS_WARN
  if (this->f_ == nullptr) {
    ESP_LOGW(TAG, "No template function set for Reactive Template Binary Sensor '%s'", this->get_name().c_str());
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


void ReactiveTemplateBinarySensor::add_to_track(binary_sensor::BinarySensor *sensor_to_add) {
  sensor_to_add->add_on_state_callback([this](float state) {
    this->execute();
  });
}



void ReactiveTemplateBinarySensor::dump_config() { LOG_BINARY_SENSOR("", "Template Binary Sensor", this); }

}  // namespace template_
}  // namespace esphome
