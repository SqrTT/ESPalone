#include "ina226_coulomb.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <cinttypes>
// originaly by ["@Sergio303", "@latonita"] - https://github.com/esphome/esphome/tree/dev/esphome/components/ina226

namespace esphome {
namespace ina226_coulomb {

static const char *const TAG = "ina226_coulomb";

// | A0   | A1   | Address |
// | GND  | GND  | 0x40    |
// | GND  | V_S+ | 0x41    |
// | GND  | SDA  | 0x42    |
// | GND  | SCL  | 0x43    |
// | V_S+ | GND  | 0x44    |
// | V_S+ | V_S+ | 0x45    |
// | V_S+ | SDA  | 0x46    |
// | V_S+ | SCL  | 0x47    |
// | SDA  | GND  | 0x48    |
// | SDA  | V_S+ | 0x49    |
// | SDA  | SDA  | 0x4A    |
// | SDA  | SCL  | 0x4B    |
// | SCL  | GND  | 0x4C    |
// | SCL  | V_S+ | 0x4D    |
// | SCL  | SDA  | 0x4E    |
// | SCL  | SCL  | 0x4F    |

static const uint8_t INA226_REGISTER_CONFIG = 0x00;
static const uint8_t INA226_REGISTER_SHUNT_VOLTAGE = 0x01;
static const uint8_t INA226_REGISTER_BUS_VOLTAGE = 0x02;
static const uint8_t INA226_REGISTER_POWER = 0x03;
static const uint8_t INA226_REGISTER_CURRENT = 0x04;
static const uint8_t INA226_REGISTER_CALIBRATION = 0x05;

static const uint16_t INA226_ADC_TIMES[] = {140, 204, 332, 588, 1100, 2116, 4156, 8244};
static const uint16_t INA226_ADC_AVG_SAMPLES[] = {1, 4, 16, 64, 128, 256, 512, 1024};

static const uint8_t SENSORS_COUNT = 7;

void INA226Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up INA226...");

  ConfigurationRegister config;

  config.reset = 1;
  if (!this->write_byte_16(INA226_REGISTER_CONFIG, config.raw)) {
    this->mark_failed();
    return;
  }
  delay(1);

  config.raw = 0;
  config.reserved = 0b100;  // as per datasheet

  // Averaging Mode AVG Bit Settings[11:9] (000 -> 1 sample, 001 -> 4 sample, 111 -> 1024 samples)
  config.avg_samples = this->adc_avg_samples_;

  // Bus Voltage Conversion Time VBUSCT Bit Settings [8:6] (100 -> 1.1ms, 111 -> 8.244 ms)
  config.bus_voltage_conversion_time = this->adc_time_voltage_;

  // Shunt Voltage Conversion Time VSHCT Bit Settings [5:3] (100 -> 1.1ms, 111 -> 8.244 ms)
  config.shunt_voltage_conversion_time = this->adc_time_current_;

  // Mode Settings [2:0] Combinations (111 -> Shunt and Bus, Continuous)
  config.mode = 0b111;

  if (!this->write_byte_16(INA226_REGISTER_CONFIG, config.raw)) {
    this->mark_failed();
    return;
  }

  // lsb is multiplied by 1000000 to store it as an integer value
  uint32_t lsb = static_cast<uint32_t>(ceilf(this->max_current_a_ * 1000000.0f / 32768));

  this->calibration_lsb_ = lsb;

  const auto calibration = uint32_t(0.00512 / (lsb * this->shunt_resistance_ohm_ / 1000000.0f));

  #ifdef ESPHOME_LOG_HAS_VERBOSE
  ESP_LOGV(TAG, "    Using LSB=%" PRIu32 " calibration=%" PRIu32, lsb, calibration);
  #endif

  if (!this->write_byte_16(INA226_REGISTER_CALIBRATION, calibration)) {
    this->mark_failed();
    return;
  }

  this->CoulombMeter::setup();

  this->disable_loop();

  this->set_interval("calcCharge", 1, [this]() {
    this->calc_charge();
  });

  this->stop_poller();

  this->set_interval("updateReports", this->get_update_interval() / SENSORS_COUNT, [this]() {
    this->report_coulomb();
  });

  this->previous_time_ = App.get_loop_component_start_time();
  this->charge_read_time_ = App.get_loop_component_start_time();
  // high_frequency_loop_requester_.start();
  //;
}

void INA226Component::dump_config() {
  ESP_LOGCONFIG(TAG, "INA226:");
  LOG_I2C_DEVICE(this);

  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with INA226 failed!");
    return;
  }
  LOG_UPDATE_INTERVAL(this);

  ESP_LOGCONFIG(TAG, "  ADC Conversion Time Bus Voltage: %d", INA226_ADC_TIMES[this->adc_time_voltage_ & 0b111]);
  ESP_LOGCONFIG(TAG, "  ADC Conversion Time Shunt Voltage: %d", INA226_ADC_TIMES[this->adc_time_current_ & 0b111]);
  ESP_LOGCONFIG(TAG, "  ADC Averaging Samples: %d", INA226_ADC_AVG_SAMPLES[this->adc_avg_samples_ & 0b111]);

  LOG_SENSOR("  ", "Bus Voltage", this->bus_voltage_sensor_);
  LOG_SENSOR("  ", "Shunt Voltage", this->shunt_voltage_sensor_);
  LOG_SENSOR("  ", "Current", this->current_sensor_);
  LOG_SENSOR("  ", "Power", this->power_sensor_);
}

float INA226Component::get_setup_priority() const { return setup_priority::DATA; }

void INA226Component::update() {}

void INA226Component::report_coulomb() {

  switch (reportCount_ % SENSORS_COUNT) {
  case 0:
    if (this->current_sensor_ != nullptr) {
      this->current_sensor_->publish_state(this->latest_current_);
    }
    break;
  case 1:
    if (this->bus_voltage_sensor_ != nullptr && latest_voltage_.has_value()) {
      this->bus_voltage_sensor_->publish_state(this->latest_voltage_.value_or(0));
    }
    break;
  case 2:
    if (this->charge_coulombs_sensor_ != nullptr) {
      this->charge_coulombs_sensor_->publish_state(this->get_charge_c());
    }
    break;
  case 3:
    if (this->power_sensor_ != nullptr && this->latest_voltage_.has_value()) {
      this->power_sensor_->publish_state(this->latest_voltage_.value_or(0) * this->latest_current_);
    }
  case 4:
    if (this->shunt_voltage_sensor_ != nullptr) {
      uint16_t raw_shunt_voltage;
      if (!this->read_byte_16(INA226_REGISTER_SHUNT_VOLTAGE, &raw_shunt_voltage)) {
        this->status_set_warning();
        return;
      }
      // Convert for 2's compliment and signed value
      float shunt_voltage_v = this->twos_complement_(raw_shunt_voltage, 16);
      shunt_voltage_v *= 0.0000025f;
      this->shunt_voltage_sensor_->publish_state(shunt_voltage_v);
    }
    break;
  case 5:
    if (this->charge_coulombs_sensor_ != nullptr) {
      this->charge_coulombs_sensor_->publish_state(this->get_charge_c());
    }
   break;
  case 6:
    if (this->read_per_second_sensor_ != nullptr) {
      auto const now = App.get_loop_component_start_time();
      auto const elapsed_s = (now - this->charge_read_time_) / 1000.0f;
      if (elapsed_s != 0) {
        const auto reads_per_second = static_cast<float>(this->charge_reads_count_) / elapsed_s;
        this->read_per_second_sensor_->publish_state(reads_per_second);
      }
      // Reset charge_reads_count_ and update charge_read_time_ to start a new measurement interval for reads-per-second calculation.
      this->charge_reads_count_ = 0;
      this->charge_read_time_ = now;
    }
    break;

  default:
    break;
  }
  reportCount_++;

  this->status_clear_warning();
}

void INA226Component::calc_charge() {
  const auto now = App.get_loop_component_start_time();

  if (now == this->previous_time_) {
    // skip if called too fast (less than 1ms)
    return;
  }
  if (reads_count_ == 0) {
    uint16_t raw_bus_voltage;
    if (this->read_byte_16(INA226_REGISTER_BUS_VOLTAGE, &raw_bus_voltage)) {
      this->latest_voltage_ = raw_bus_voltage * 0.00125f * this->bus_voltage_calibration_;
    }
  }
  reads_count_++;
  if (reads_count_ > 20) {
    reads_count_ = 0;
  };

  uint16_t raw_current;
  if (!this->read_byte_16(INA226_REGISTER_CURRENT, &raw_current)) {
    this->status_set_warning();
    return;
  }
  
  // Convert for 2's compliment and signed value
  const auto current_a = (this->twos_complement_(raw_current, 16) * (this->calibration_lsb_ / 1000.0f)) / 1000.0f;

  this->latest_current_ = current_a;
  const auto delta_mc = current_a * (now - this->previous_time_);

  this->partial_charge_mc_ += delta_mc; 
  
  const int64_t delta_int = (int64_t)this->partial_charge_mc_;
  this->latest_charge_mc_ += delta_int;
  this->partial_charge_mc_ -= delta_int;
  
  this->partial_energy_mj_ += this->latest_voltage_.value_or(0) * delta_mc; 
  const int64_t energy_int = (int64_t)this->partial_energy_mj_;
  this->latest_energy_mj_ += energy_int;
  this->partial_energy_mj_ -= energy_int;

  this->previous_time_ = now;

  this->charge_reads_count_++;
}

int32_t INA226Component::twos_complement_(int32_t val, uint8_t bits) {
  if (val & ((uint32_t) 1 << (bits - 1))) {
    val -= (uint32_t) 1 << bits;
  }
  return val;
}

}  // namespace ina226
}  // namespace esphome
