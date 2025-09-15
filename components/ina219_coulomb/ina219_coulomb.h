#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "../coulomb_meter/coulomb_meter.h"
#include <cinttypes>

namespace esphome {
namespace ina219_coulomb {

class INA219Component : public i2c::I2CDevice, public coulomb_meter::CoulombMeter  {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;
  void on_powerdown() override;

  void calc_charge();
  void report_coulomb();

  void set_shunt_resistance_ohm(float shunt_resistance_ohm) { shunt_resistance_ohm_ = shunt_resistance_ohm; }
  void set_max_current_a(float max_current_a) { max_current_a_ = max_current_a; }
  void set_max_voltage_v(float max_voltage_v) { max_voltage_v_ = max_voltage_v; }
  void set_bus_voltage_sensor(sensor::Sensor *bus_voltage_sensor) { bus_voltage_sensor_ = bus_voltage_sensor; }
  void set_shunt_voltage_sensor(sensor::Sensor *shunt_voltage_sensor) { shunt_voltage_sensor_ = shunt_voltage_sensor; }
  void set_current_sensor(sensor::Sensor *current_sensor) { current_sensor_ = current_sensor; }
  void set_power_sensor(sensor::Sensor *power_sensor) { power_sensor_ = power_sensor; }
  void set_charge_coulombs_sensor(sensor::Sensor *charge_coulombs_sensor) { charge_coulombs_sensor_ = charge_coulombs_sensor; }
  void set_read_per_second_sensor(sensor::Sensor *read_per_second_sensor) { read_per_second_sensor_ = read_per_second_sensor; }

  float get_voltage() override { return latest_voltage_.value_or(0);  };
  float get_current() override { return latest_current_;  };
  int64_t get_charge_c() override { return latest_charge_mc_ / 1000; } ;
  int64_t get_energy_j() override { return latest_energy_mj_ / 1000; } ;

 protected:
  int64_t latest_energy_mj_{0};
  int64_t latest_charge_mc_{0};

  sensor::Sensor *bus_voltage_sensor_{nullptr};
  sensor::Sensor *shunt_voltage_sensor_{nullptr};
  sensor::Sensor *current_sensor_{nullptr};
  sensor::Sensor *power_sensor_{nullptr};
  sensor::Sensor *charge_coulombs_sensor_{nullptr};
  sensor::Sensor *read_per_second_sensor_{nullptr};
 
  optional<float> latest_voltage_;
  float shunt_resistance_ohm_;
  float max_current_a_;
  float max_voltage_v_;
  float latest_current_{0};

  float partial_energy_mj_{0};

  float partial_charge_mc_{0};

  uint32_t reads_count_{0};
  uint32_t previous_time_{0};
 
  uint32_t charge_reads_count_{0};
  uint32_t charge_read_time_{0};
  uint32_t calibration_lsb_;
};

}  // namespace ina219
}  // namespace esphome
