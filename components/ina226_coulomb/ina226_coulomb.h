#pragma once

// originaly by ["@Sergio303", "@latonita"] - https://github.com/esphome/esphome/tree/dev/esphome/components/ina226
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "../coulomb_meter/coulomb_meter.h"


namespace esphome {
namespace ina226_coulomb {

enum AdcTime : uint16_t {
  ADC_TIME_140US = 0,
  ADC_TIME_204US = 1,
  ADC_TIME_332US = 2,
  ADC_TIME_588US = 3,
  ADC_TIME_1100US = 4,
  ADC_TIME_2116US = 5,
  ADC_TIME_4156US = 6,
  ADC_TIME_8244US = 7
};

enum AdcAvgSamples : uint16_t {
  ADC_AVG_SAMPLES_1 = 0,
  ADC_AVG_SAMPLES_4 = 1,
  ADC_AVG_SAMPLES_16 = 2,
  ADC_AVG_SAMPLES_64 = 3,
  ADC_AVG_SAMPLES_128 = 4,
  ADC_AVG_SAMPLES_256 = 5,
  ADC_AVG_SAMPLES_512 = 6,
  ADC_AVG_SAMPLES_1024 = 7
};

union ConfigurationRegister {
  uint16_t raw;
  struct {
    uint16_t mode : 3;
    AdcTime shunt_voltage_conversion_time : 3;
    AdcTime bus_voltage_conversion_time : 3;
    AdcAvgSamples avg_samples : 3;
    uint16_t reserved : 3;
    uint16_t reset : 1;
  } __attribute__((packed));
};

class INA226Component : public i2c::I2CDevice, public coulomb_meter::CoulombMeter  {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override;
  void report_coulomb();
  void calc_charge();

  void set_shunt_resistance_ohm(float shunt_resistance_ohm) { shunt_resistance_ohm_ = shunt_resistance_ohm; }
  void set_max_current_a(float max_current_a) { max_current_a_ = max_current_a; }
  void set_adc_time_voltage(AdcTime time) { adc_time_voltage_ = time; }
  void set_adc_time_current(AdcTime time) { adc_time_current_ = time; }
  void set_adc_avg_samples(AdcAvgSamples samples) { adc_avg_samples_ = samples; }
  void set_high_frequency_loop() {
    HighFrequencyLoopRequester high_frequency_loop_requester;
     high_frequency_loop_requester.start();
  };

  void set_bus_voltage_sensor(sensor::Sensor *bus_voltage_sensor) { bus_voltage_sensor_ = bus_voltage_sensor; }
  void set_bus_voltage_calibration(float calibration) { bus_voltage_calibration_ = calibration; }
  void set_shunt_voltage_sensor(sensor::Sensor *shunt_voltage_sensor) { shunt_voltage_sensor_ = shunt_voltage_sensor; }
  void set_current_sensor(sensor::Sensor *current_sensor) { current_sensor_ = current_sensor; }
  void set_power_sensor(sensor::Sensor *power_sensor) { power_sensor_ = power_sensor; }
  void set_charge_coulombs_sensor(sensor::Sensor *power_sensor) { charge_coulombs_sensor_ = power_sensor; }
  void set_read_per_second_sensor(sensor::Sensor *read_per_second_sensor) { read_per_second_sensor_ = read_per_second_sensor; }

  float get_voltage() override { return latest_voltage_.value_or(0);  };
  float get_current() override { return latest_current_;  };
  int64_t get_charge_c() override { return latest_charge_mc_ / 1000; } ;
  int64_t get_energy_j() override { return latest_energy_mj_ / 1000; } ;

 protected:
  int64_t latest_energy_mj_{0};
  int64_t latest_charge_mc_{0};
  float shunt_resistance_ohm_;
  float max_current_a_;
  AdcTime adc_time_voltage_{AdcTime::ADC_TIME_1100US};
  AdcTime adc_time_current_{AdcTime::ADC_TIME_1100US};
  AdcAvgSamples adc_avg_samples_{AdcAvgSamples::ADC_AVG_SAMPLES_4};
  uint32_t calibration_lsb_;
  sensor::Sensor *bus_voltage_sensor_{nullptr};
  sensor::Sensor *shunt_voltage_sensor_{nullptr};
  sensor::Sensor *current_sensor_{nullptr};
  sensor::Sensor *power_sensor_{nullptr};
  sensor::Sensor *charge_coulombs_sensor_{nullptr};
  sensor::Sensor *read_per_second_sensor_{nullptr};
  
  float bus_voltage_calibration_{1};

  int32_t twos_complement_(int32_t val, uint8_t bits);

  optional<float> latest_voltage_;
  float latest_current_{0};

  float partial_energy_mj_{0};

  float partial_charge_mc_{0};

  uint32_t reads_count_{0};
  uint32_t previous_time_{0};
 
  uint32_t charge_reads_count_{0};
  uint32_t charge_read_time_{0};
};

}  // namespace ina226_coulomb
}  // namespace esphome
