#pragma once

#include "esphome/core/component.h"
#include "esphome/core/application.h"

#include "esphome/components/sensor/sensor.h"
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome {
namespace battery_charger {


  class InternalTimer {
    public:
    void setup(Component *component, u_int32_t time, const char *name) {
      this->component_ = component;
      this->name_ = name;
      this->time_s = time;
    }

    void start(std::function<void()> func) {
      if (!this->is_running_ && this->time_s != 0) {
        if (this->component_ == nullptr) {
          ESP_LOGW("BatteryCharger", "Tried to starting uninitialized timer '%s' for %i sec", this->name_, this->time_s);
          return;
        }
        ESP_LOGV("BatteryCharger", "Starting timer '%s' for %i sec", this->name_, this->time_s);
        App.scheduler.set_timeout(this->component_, this->name_, this->time_s * 1000, [this, func](){
          ESP_LOGV("BatteryCharger", "Fired timer '%s' after %i sec", this->name_, this->time_s);
          this->is_running_ = false;
          func();
        });
        this->is_running_ = true;
      }
    }

    void stop() {
      if (this->is_running_) {
        ESP_LOGV("BatteryCharger", "Stoping timer '%s'", this->name_);
        this->is_running_ = false;
        App.scheduler.cancel_timeout(this->component_, this->name_);
      }
    }

    u_int32_t time_s{0};
    protected:
      Component* component_;
      const char *name_;
      bool is_running_{false};
  };

  enum CHARGE_STATES {
    INITIAL,
    BEFORE_ABSORPTION,
    ABSORPTION,
    BEFORE_FLOAT,
    FLOAT,
    BEFORE_EQUAIZATION,
    EQUAIZATION,
    BEFORE_ERROR,
    ERROR,
  };
class ChargerComponent : public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  // void loop() override;

  void set_float_voltage(float float_voltage) { float_voltage_ = float_voltage; };

  void set_voltage_sensor(sensor::Sensor *sensor) { voltage_sensor_ = sensor; };
  void set_current_sensor(sensor::Sensor *sensor) { current_sensor_ = sensor; };
  #ifdef USE_TEXT_SENSOR
  void set_charge_state_sensor(text_sensor::TextSensor *sensor) { charge_state_sensor_ = sensor; };
  #endif
  void set_voltage_target_sensor(sensor::Sensor *sensor) { voltage_target_sensor_ = sensor; };


  void set_absorption_time(u_int32_t time) { this->absorption_timer_.setup(this, time, "ABSORPTION_TIMER"); };
  void set_absorption_voltage(float voltage) { absorption_voltage_v_ = voltage; };
  void set_absorption_restart_voltage(float voltage) { absorption_restart_voltage_v_ = voltage; };
  void set_absorption_current(float current) { absorption_current_a_ = current; };
  void set_absorption_restart_time(u_int32_t time) { this->absorption_restart_timer_.setup(this, time, "ABSORPTION_TIMER_RESTART"); };
  void set_absorption_low_voltage_delay_s_(u_int32_t time) { this->absorption_low_voltage_timer_.setup(this, time, "ABSORPTION_LOW_VOLTAGE"); };


  void set_equaization_time(u_int32_t time) { this->equaization_timer_.setup(this, time, "EQUAIZATION_TIME"); };
  void set_equaization_interval(u_int32_t time) { this->equaization_interval_timer_.setup(this, time, "EQUAIZATION_INTERVAL_TIMER"); };
  void set_equaization_timeout(u_int32_t time) { this->equaization_timeout_timer_.setup(this, time, "EQUAIZATION_TIMEOUT"); };
  void set_equaization_voltage(float voltage) { equaization_voltage_v_ = voltage; };

  void set_max_voltage(float voltage) { max_voltage_ = voltage; };
  void set_min_voltage(float voltage) { min_voltage_ = voltage; };
  void set_voltage_auto_recovery_delay(u_int32_t time) { this->voltage_auto_recovery_delay_timer_.setup(this, time, "AUTO_RECOVERY");};

 protected:
    void updateState();
    void call_update_state_later(CHARGE_STATES new_state);
    CHARGE_STATES charge_state_{INITIAL};

    optional<float> absorption_voltage_v_;
    optional<float> absorption_current_a_;
    InternalTimer absorption_restart_timer_;
    
    optional<float> absorption_restart_voltage_v_;
    InternalTimer absorption_timer_;
    InternalTimer absorption_low_voltage_timer_;

    optional<float> equaization_voltage_v_;
    InternalTimer equaization_timeout_timer_;
    InternalTimer equaization_interval_timer_;
    InternalTimer equaization_timer_;
    
    sensor::Sensor *voltage_sensor_{nullptr};
    sensor::Sensor *current_sensor_{nullptr};
    sensor::Sensor *voltage_target_sensor_{nullptr};
    #ifdef USE_TEXT_SENSOR
    text_sensor::TextSensor *charge_state_sensor_{nullptr};
    #endif
    optional<float> float_voltage_;

    optional<float> last_current_;
    float last_voltage_{0};

    optional<float> max_voltage_;
    optional<float> min_voltage_;
  
    InternalTimer voltage_auto_recovery_delay_timer_;
};

}  // namespace baterry_charger
}  // namespace esphome
