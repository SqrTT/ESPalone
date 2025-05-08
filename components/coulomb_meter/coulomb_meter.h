#pragma once

#include "esphome/core/component.h"
#include "esphome/core/application.h"
#include <optional>  

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
    Component* component_{nullptr};
    const char *name_{nullptr};
    bool is_running_{false};
};

class MovingAvarage {
  public:

    void setup(u_int8_t size) {
      this->size_ = size;
      this->values_.resize(size);
    }

    void add(const int32_t value) {
      this->values_[this->index_] = value;
      this->index_ = (this->index_ + 1) % this->size_;

      if (count_ < size_) {
        count_++;
      }
    }

    float get() {
      if (count_ == 0) {
        return 0.0;
      }
      
      int32_t sum = 0;
      for (int i = 0; i < this->size_; i++) {
        sum += this->values_[i];
      }
      
      return sum / count_;
    }

  private:
    int size_;
    int index_{0};
    int count_{0};
    std::vector<int32_t> values_;
};

class CoulombMeter : public PollingComponent {
 public:
  // CoulombMeter() : PollingComponent(0), energy_usage_average_(6) {};
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
  void set_full_energy(float energy) { full_energy_j_ = energy * 3600; };

  void set_charge_level_sensor(sensor::Sensor *sensor) { charge_level_sensor_ = sensor; };
  void set_charge_out_sensor(sensor::Sensor *sensor) { charge_out_sensor_ = sensor; };
  void set_charge_in_sensor(sensor::Sensor *sensor) { charge_in_sensor_ = sensor; };
  void set_charge_remaining_sensor(sensor::Sensor *sensor) { charge_remaining_sensor_ = sensor; };

  void set_energy_level_sensor(sensor::Sensor *sensor) { energy_level_sensor_ = sensor; };
  void set_energy_remaining_sensor(sensor::Sensor *sensor) { energy_remaining_sensor_ = sensor; };
  void set_energy_in_sensor(sensor::Sensor *sensor) { energy_in_sensor_ = sensor; };
  void set_energy_out_sensor(sensor::Sensor *sensor) { energy_out_sensor_ = sensor; };

  void set_charge_time_remaining_sensor(sensor::Sensor *sensor) { charge_time_remaining_sensor_ = sensor; };
  void set_discharge_time_remaining_sensor(sensor::Sensor *sensor) { discharge_time_remaining_sensor_ = sensor; };
  
  virtual float get_voltage();
  virtual float get_current();
  virtual int64_t get_charge_c();
  virtual int64_t get_energy_j();

 protected:
    bool updateSensors();
    void updateState();

    void publish_state_(sensor::Sensor *sensor, float value);

    // int16_t current_charge_level_{0};
    // int16_t current_energy_level_{0};

    float fully_charge_voltage_{0};
    optional<float> fully_charge_current_;
    InternalTimer fully_charge_time_;
    bool full_charge_reached_{false};
    
    float fully_discharge_voltage_v_{0};
    InternalTimer fully_discharge_timer_;
    bool full_discharge_reached_{false};

    int32_t full_capacity_c_{0};
    int32_t full_energy_j_{0};
    optional<int32_t> full_charge_calculated_c_;
    ESPPreferenceObject pref_full_charge_calculated_c_;
    optional<int32_t> full_energy_calculated_j_;
    ESPPreferenceObject pref_full_energy_calculated_j_;
    int32_t current_charge_c_{0};
    ESPPreferenceObject pref_current_charge_c_;
    int32_t current_energy_j_{0};
    ESPPreferenceObject pref_current_energy_j_;

    int64_t previous_charge_c_{0};
    int64_t previous_energy_j_{0};
    uint64_t cumulative_charge_in_c_{0};
    uint64_t cumulative_energy_in_j_{0};
    optional<uint64_t> cumulative_at_fill_charge_c_;
    optional<uint64_t> cumulative_at_fill_charge_j_;
    uint64_t cumulative_charge_out_c_{0};
    uint64_t cumulative_energy_out_j_{0};
    optional<uint64_t> cumulative_at_full_discharge_c_;
    //optional<uint64_t> cumulative_at_full_discharge_j_;
    
    sensor::Sensor *charge_level_sensor_{nullptr};
    sensor::Sensor *charge_out_sensor_{nullptr};
    sensor::Sensor *charge_in_sensor_{nullptr};
    sensor::Sensor *charge_remaining_sensor_{nullptr};

    sensor::Sensor *energy_level_sensor_{nullptr};
    sensor::Sensor *energy_remaining_sensor_{nullptr};
    sensor::Sensor *energy_in_sensor_{nullptr};
    sensor::Sensor *energy_out_sensor_{nullptr};

    sensor::Sensor *charge_time_remaining_sensor_{nullptr};
    sensor::Sensor *discharge_time_remaining_sensor_{nullptr};
    MovingAvarage energy_usage_average_;
    int32_t prev_time_energy_j_{0};

    enum class State : uint8_t {
      NOT_INITIALIZED = 0x0,
      IDLE,
      SETUP,
      START,
      CHARGE_LEVEL_SENSOR,
      CHARGE_OUT_SENSOR,
      CHARGE_IN_SENSOR,
      CHARGE_REMAINING_SENSOR,
      ENERGY_LEVEL_SENSOR,
      ENERGY_IN_SENSOR,
      ENERGY_OUT_SENSOR,
      ENERGY_REMAINING_SENSOR,
      TIME_REMAINING_SENSOR,
    } meter_state_{State::NOT_INITIALIZED};
};

}  // namespace coulomb_meter
}  // namespace esphome
