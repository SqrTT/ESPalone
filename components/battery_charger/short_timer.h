#pragma once

#include "esphome/core/component.h"
#include "esphome/core/application.h"

#include "esphome/components/sensor/sensor.h"
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome {
namespace short_timer {

  class ShortTimer {
    public:
    void setup(u_int16_t time, const char *name) {
      this->name_ = name;
      this->time_setup_s_ = time;
      time_left_s_ = global_preferences->make_preference<int16_t>(fnv1_hash(name), false);
    }

    void start(std::function<void()> func) {
      if (!this->is_running_) {
        time_left_s_.save(&this->time_setup_s_);
        func_ = func;
        ESP_LOGV("BatteryCharger", "Starting timer '%s' for %i sec", this->name_, this->time_setup_s_);
        this->is_running_ = true;
      }
    }

    void stop() {
      if (this->is_running_) {
        ESP_LOGV("BatteryCharger", "Stoping timer '%s'", this->name_);
        this->is_running_ = false;
        uint16_t time_left = 0;
        time_left_s_.save(&time_left);
      }
    }

    // execute every second
    void loop() {
      if (!this->is_running_) {
        return;
      }
      //  App.get_loop_component_start_time();
      // Nothing needed here for now
    }

    private:
      ESPPreferenceObject time_left_s_{nullptr};
      std::function<void()> func_{nullptr};
      const char *name_{nullptr};
      u_int16_t time_setup_s_{0};
      bool is_running_{false};
  };

}  // namespace short_timer
}  // namespace esphome
