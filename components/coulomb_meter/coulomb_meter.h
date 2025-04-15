#pragma once

#include "esphome/core/component.h"
#include "esphome/core/application.h"


namespace esphome {
namespace coulomb_meter {


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


class CoulombMeter : public PollingComponent {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override;
  // void loop() override;

  void set_fully_charge_voltage(float voltage) { fully_charge_voltage_ = voltage; };
  void set_fully_charge_current(float voltage) { fully_charge_current_ = voltage; };
  void set_fully_charge_time(u_int32_t time) { this->fully_charge_time_.setup(this, time, "CHARGE_TIMER");  };

  void set_fully_discharge_voltage(float voltage) { fully_discharge_voltage_v_ = voltage; };
  void set_fully_discharge_time(u_int32_t time) { this->fully_discharge_timer_.setup(this, time, "DISCHARGE_TIMER"); };

  void set_full_capacity(float capacity) { full_capacity_c_ = capacity * 3600; };

  void set_soc_target_sensor(sensor::Sensor *sensor) { soc_target_sensor_ = sensor; };
  void set_discharge_sensor(sensor::Sensor *sensor) { discharge_sensor_ = sensor; };
  void set_charge_sensor(sensor::Sensor *sensor) { charge_sensor_ = sensor; };
  void set_remaining_sensor(sensor::Sensor *sensor) { remaining_sensor_ = sensor; };
  virtual float getVoltage();
  virtual float getCurrent();
  virtual int64_t getCharge_c();

 protected:
    void updateState();

    int16_t current_soc_{0};

    float fully_charge_voltage_{0};
    optional<float> fully_charge_current_;
    InternalTimer fully_charge_time_;
    bool full_charge_reached_{false};
    
    float fully_discharge_voltage_v_{0};
    InternalTimer fully_discharge_timer_;
    bool full_discharge_reached_{false};

    enum class State : uint8_t {
      NOT_INITIALIZED = 0x0,
      IDLE,
      DATA_CALCULATE_CHARGE
    } meter_state_{State::NOT_INITIALIZED};
   
    int32_t full_capacity_c_{0};
    optional<int32_t> full_capacity_calculated_c_;
    int32_t current_charge_c_{0};

    int64_t previous_charge_c_{0};
    uint64_t cumulative_charge_c_{0};
    optional<uint64_t> cumulative_at_fill_charge_c_;
    uint64_t cumulative_discharge_c_{0};
    optional<uint64_t> cumulative_at_full_discharge_c_;
    
    sensor::Sensor *soc_target_sensor_{nullptr};
    sensor::Sensor *discharge_sensor_{nullptr};
    sensor::Sensor *charge_sensor_{nullptr};
    sensor::Sensor *remaining_sensor_{nullptr};
};

}  // namespace coulomb_meter
}  // namespace esphome
