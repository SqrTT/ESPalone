#include "coulomb_meter.h"

// .platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-addr2line -pfiaC -e esphome_config/.esphome/build/ina226coulomb/.pioenvs/ina226coulomb/firmware.elf  0x4022b8e2
namespace esphome {
  namespace coulomb_meter {

    static const char *const TAG = "CoulombMeter";
    static const unsigned int TIME_REMAINING = 30000; // ms

    int32_t clamp_map(int32_t x, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max)
    {
      // ESP_LOGV(TAG, "Clamp map: %i %i %i %i %i", x, in_min, in_max, out_min, out_max);
      if (x <= in_min)
        return out_min;
      if (x >= in_max)
        return out_max;

      // Prevent division by zero
      if (in_max == in_min) {
          ESP_LOGE(TAG, "clamp_map: Division by zero detected (in_max == in_min)");
          return out_min; // Return a safe default value
      }
      return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    }

    void CoulombMeter::setup() {
      this->set_interval("updateStatus", 1000, [this]() { 
        this->updateState(); 
      });
      if (charge_time_remaining_sensor_ != nullptr || discharge_time_remaining_sensor_ != nullptr) {
        this->energy_usage_average_.setup(10);
        this->set_interval("updateTimeRemaining", TIME_REMAINING, [this]() { 
          const auto delta_energy = this->current_energy_j_ - this->prev_time_energy_j_;
          this->prev_time_energy_j_ = this->current_energy_j_;

          this->energy_usage_average_.add(delta_energy);
        });
      }

      rtc_current_charge_c_ = global_preferences->make_preference<int32_t>(fnv1_hash("current_charge_c"), false);
      rtc_current_energy_j_ = global_preferences->make_preference<int32_t>(fnv1_hash("current_energy_j"), false);

      rtc_cumulative_at_full_in_c_ = global_preferences->make_preference<uint64_t>(fnv1_hash("cumulative_at_full_in_c_"), false);
      rtc_cumulative_at_full_in_j_ = global_preferences->make_preference<uint64_t>(fnv1_hash("cumulative_at_full_in_j_"), false);
      rtc_cumulative_at_full_out_c_ = global_preferences->make_preference<uint64_t>(fnv1_hash("cumulative_at_full_out_c_"), false);
      rtc_cumulative_at_full_out_j_ = global_preferences->make_preference<uint64_t>(fnv1_hash("cumulative_at_full_out_j_"), false);
      
      rtc_cumulative_charge_in_c_ = global_preferences->make_preference<uint64_t>(fnv1_hash("cumulative_charge_in_c_"), false);
      rtc_cumulative_energy_in_j_ = global_preferences->make_preference<uint64_t>(fnv1_hash("cumulative_energy_in_j_"), false);
      rtc_cumulative_charge_out_c_ = global_preferences->make_preference<uint64_t>(fnv1_hash("cumulative_charge_out_c_"), false);
      rtc_cumulative_energy_out_j_ = global_preferences->make_preference<uint64_t>(fnv1_hash("cumulative_energy_out_j_"), false);

      flash_full_charge_calculated_c_ = global_preferences->make_preference<int32_t>(fnv1_hash("CoulombMeter_full_charge_calculated_c"), true);
      flash_full_energy_calculated_j_ = global_preferences->make_preference<int32_t>(fnv1_hash("CoulombMeter_full_energy_calculated_j"), true);

      uint64_t tmp_cumulative_charge_in_c_ = 0;
      if (rtc_cumulative_charge_in_c_.load(&tmp_cumulative_charge_in_c_)) {
        cumulative_charge_in_c_ = tmp_cumulative_charge_in_c_;
      }

      uint64_t tmp_cumulative_energy_in_j_ = 0;
      if (rtc_cumulative_energy_in_j_.load(&tmp_cumulative_energy_in_j_)) {
        cumulative_energy_in_j_ = tmp_cumulative_energy_in_j_;
      }

      uint64_t tmp_cumulative_charge_out_c_ = 0;
      if (rtc_cumulative_charge_out_c_.load(&tmp_cumulative_charge_out_c_)) {
        cumulative_charge_out_c_ = tmp_cumulative_charge_out_c_;
      }

      uint64_t tmp_cumulative_energy_out_j_ = 0;
      if (rtc_cumulative_energy_out_j_.load(&tmp_cumulative_energy_out_j_)) {
        cumulative_energy_out_j_ = tmp_cumulative_energy_out_j_;
      }

      int32_t charge_calculated_c_ = 0;
      if (flash_full_charge_calculated_c_.load(&charge_calculated_c_)) {
        ESP_LOGD(TAG, "Loaded full charge from flash: %i", charge_calculated_c_);
        full_charge_calculated_c_ = charge_calculated_c_;
      } else {
        ESP_LOGD(TAG, "Failed to load full charge, use default");
      };
      int32_t energy_calculated_j_;
      if (flash_full_energy_calculated_j_.load(&energy_calculated_j_)) {
        ESP_LOGD(TAG, "Loaded full energy from flash: %i", energy_calculated_j_);
        full_energy_calculated_j_ = energy_calculated_j_;
      } else {
        ESP_LOGD(TAG, "Failed to load full energy, use default");
      };

      int32_t charge;
      if (rtc_current_charge_c_.load(&charge)) {
        ESP_LOGD(TAG, "Loaded current charge from rtc: %i", charge);
        current_charge_c_ = charge;
      }

      int32_t energy;
      if (rtc_current_energy_j_.load(&energy)) {
        ESP_LOGD(TAG, "Loaded current energy from rtc: %i", energy);
        current_energy_j_ = energy;
      }

      this->set_interval("saveCounters", 15 * 1000, [this]() { 
        this->storeCounters(); 
      });
    }

    void CoulombMeter::storeCounters() {
      rtc_current_energy_j_.save(&current_energy_j_);
      rtc_current_charge_c_.save(&current_charge_c_);

      rtc_cumulative_charge_in_c_.save(&cumulative_charge_in_c_);
      rtc_cumulative_energy_in_j_.save(&cumulative_energy_in_j_);
      rtc_cumulative_charge_out_c_.save(&cumulative_charge_out_c_);
      rtc_cumulative_energy_out_j_.save(&cumulative_energy_out_j_);
    }

    void CoulombMeter::on_shutdown() {
      storeCounters();
    }

    void CoulombMeter::update() {
      if (this->meter_state_ == State::IDLE) {
        this->meter_state_ = State::START;
      }
    }

    void CoulombMeter::updateState() {
      if (this->meter_state_ == State::NOT_INITIALIZED) {
        return;
      }
      const auto voltage = this->get_voltage();

      if (this->meter_state_ == State::SETUP) {

        if (current_charge_c_ == 0) {
          ESP_LOGD(TAG, "Failed to load current charge, use voltage to determinate charge level");
          // use initial charge based on voltage
          current_charge_c_ = clamp_map(
            voltage * 1000,
            fully_discharge_voltage_v_ * 1000,
            fully_charge_voltage_ * 1000,
            0,
            full_charge_calculated_c_.value_or(full_capacity_c_)
          );
        }

        if (current_energy_j_ == 0) {
          ESP_LOGD(TAG, "Failed to load current energy, use voltage to determinate charge level");
          current_energy_j_ = clamp_map(
            voltage * 1000,
            fully_discharge_voltage_v_ * 1000,
            fully_charge_voltage_ * 1000,
            0,
            full_energy_calculated_j_.value_or(full_energy_j_)
          );
        }
 
        prev_time_energy_j_ = current_energy_j_;
        this->meter_state_ = State::IDLE;
      }

      const auto charge = this->get_charge_c();

      int32_t delta_charge = charge - this->previous_charge_c_;
      this->previous_charge_c_ = charge;

      if (delta_charge > 0) {
        this->cumulative_charge_in_c_ += delta_charge;
      } else if (delta_charge < 0) {
        this->cumulative_charge_out_c_ += -delta_charge;
      }
      current_charge_c_ += delta_charge;

      if (current_charge_c_ < 0) {
        current_charge_c_ = 0;
      } else if (current_charge_c_ > full_charge_calculated_c_.value_or(full_capacity_c_)) {
        current_charge_c_ = full_charge_calculated_c_.value_or(full_capacity_c_);
      }

      if (full_charge_reached_ || full_discharge_reached_) {
        const auto current_charge_level_ = clamp_map(
          this->current_charge_c_,
          0,
          full_charge_calculated_c_.value_or(full_capacity_c_),
          0,
          100
        );
        if (full_charge_reached_ && current_charge_level_ < 99) {
          full_charge_reached_ = false;
        }
        if (full_discharge_reached_ && current_charge_level_ > 1) {
          full_discharge_reached_ = false;
        }
      }

      const auto energy_j = this->get_energy_j(); 

      const auto delta_energy = energy_j - this->previous_energy_j_;

      this->previous_energy_j_ = energy_j;
      if (delta_energy > 0) {
        this->cumulative_energy_in_j_ += delta_energy;
      } else if (delta_energy < 0) {
        this->cumulative_energy_out_j_ += -delta_energy;
      }
      current_energy_j_ += delta_energy;
      if (current_energy_j_ < 0) {
        current_energy_j_ = 0;
      } else if (current_energy_j_ > full_energy_calculated_j_.value_or(full_energy_j_)) {
        current_energy_j_ = full_energy_calculated_j_.value_or(full_energy_j_);
      }

      // is fully charged?
      if (full_charge_reached_ == false && voltage >= fully_charge_voltage_ && (fully_charge_current_.has_value() ? this->get_current() <= fully_charge_current_.value() : true)) {
        fully_charge_time_.start([this]() {
          full_charge_reached_ = true;
          this->current_charge_c_ = full_charge_calculated_c_.value_or(full_capacity_c_);
          this->current_energy_j_ = full_energy_calculated_j_.value_or(full_energy_j_);
          ESP_LOGD(TAG, "Full charge reached: %i", this->current_charge_c_);

          rtc_cumulative_at_full_in_c_.save(&cumulative_charge_in_c_);
          rtc_cumulative_at_full_in_j_.save(&cumulative_energy_in_j_);
          rtc_cumulative_at_full_out_c_.save(&cumulative_charge_out_c_);
          rtc_cumulative_at_full_out_j_.save(&cumulative_energy_out_j_);
        });
      } else {
        fully_charge_time_.stop();
      }

      // is fully discharged?
      if (voltage <= fully_discharge_voltage_v_ && full_discharge_reached_ == false) {
          fully_discharge_timer_.start([this]() {
          full_discharge_reached_ = true;
          this->current_charge_c_ = 0;
          this->current_energy_j_ = 0;
          // calculate capacity based on charge
          uint64_t cumulative_at_full_in_c_ = 0;
          uint64_t cumulative_at_full_out_c_ = 0;

          if (
            rtc_cumulative_at_full_in_c_.load(&cumulative_at_full_in_c_) &&
            rtc_cumulative_at_full_out_c_.load(&cumulative_at_full_out_c_)
          ) {
            const auto charge_delta = cumulative_charge_in_c_ - cumulative_at_full_in_c_;
            const auto discharge_delta = cumulative_charge_out_c_ - cumulative_at_full_out_c_;
            const auto capacity = charge_delta + discharge_delta;

            ESP_LOGD(TAG, "Capacity calculated: %i", capacity);
            ESP_LOGD(TAG, "Charge delta: %i", charge_delta);
            ESP_LOGD(TAG, "Discharge delta: %i", discharge_delta);
            if (capacity > 0) {
              full_charge_calculated_c_ = capacity;
              int32_t stored_capacity = 0;
              if (flash_full_charge_calculated_c_.load(&stored_capacity)) {
                if (stored_capacity != full_charge_calculated_c_) {
                  ESP_LOGD(TAG, "Saving full charge: %i", full_charge_calculated_c_);
                  flash_full_charge_calculated_c_.save(&full_charge_calculated_c_);
                }
              } else {
                flash_full_charge_calculated_c_.save(&full_charge_calculated_c_);
              }
              ESP_LOGD(TAG, "Capacity calculated: %i", capacity);
            } else {
              ESP_LOGW(TAG, "Capacity invalid: %i", capacity);
            }
          }
          // calculate energy based on energy
          uint64_t cumulative_at_full_in_j_ = 0;
          uint64_t cumulative_at_full_out_j_ = 0;

          if (
            rtc_cumulative_at_full_in_j_.load(&cumulative_at_full_in_j_) &&
            rtc_cumulative_at_full_out_j_.load(&cumulative_at_full_out_j_)
          ) {
            const auto energy_delta = cumulative_energy_in_j_ - cumulative_at_full_in_j_;
            const auto discharge_delta = cumulative_energy_out_j_ - cumulative_at_full_out_j_;
            const auto energy_capacity = energy_delta + discharge_delta;

            ESP_LOGD(TAG, "Energy capacity calculated: %i", energy_capacity);
            ESP_LOGD(TAG, "Energy delta: %i", energy_delta);
            ESP_LOGD(TAG, "Discharge delta: %i", discharge_delta);
            if (energy_capacity > 0) {
              full_energy_calculated_j_ = energy_capacity;
              int32_t stored_energy = 0;
              if (flash_full_energy_calculated_j_.load(&stored_energy)) {
                if (stored_energy != full_energy_calculated_j_) {
                  ESP_LOGD(TAG, "Saving full energy: %i", full_energy_calculated_j_);
                  flash_full_energy_calculated_j_.save(&full_energy_calculated_j_);
                }
              } else {
                flash_full_energy_calculated_j_.save(&full_energy_calculated_j_);
              }
              ESP_LOGD(TAG, "Energy capacity calculated: %i", energy_capacity);
            } else {
              ESP_LOGW(TAG, "Energy capacity invalid: %i", energy_capacity);
            }
          }
        });
      } else {
        // if voltage is above fully discharge voltage, stop timer
        fully_discharge_timer_.stop();
      }
    }

    bool CoulombMeter::updateSensors() {
      switch (this->meter_state_) {
        case State::NOT_INITIALIZED:
          this->meter_state_ = State::SETUP;
          return true;
          break;
        case State::SETUP:
        case State::IDLE:
          return true;
          break;
        case State::START:
        case State::CHARGE_LEVEL_SENSOR:
          this->meter_state_ = State::CHARGE_OUT_SENSOR;
          if (charge_level_sensor_ != nullptr) {
            const auto current_charge_level_ = std::min(
                std::max(
                  clamp_map(
                    this->current_charge_c_,
                    0,
                    full_charge_calculated_c_.value_or(full_capacity_c_),
                    0,
                    100
                  ),
                  full_discharge_reached_ ? 0 : 1
                ),
                full_charge_reached_ ? 100 : 99
            );
            

            publish_state_(charge_level_sensor_, current_charge_level_);
          } 
          return false;
          break;
        case State::CHARGE_OUT_SENSOR:
          this->meter_state_ = State::CHARGE_IN_SENSOR;

          publish_state_(charge_out_sensor_, this->cumulative_charge_out_c_ / 3600.0f);

          return false;
          break;
        case State::CHARGE_IN_SENSOR:
          this->meter_state_ = State::CHARGE_REMAINING_SENSOR;
          publish_state_(charge_in_sensor_, this->cumulative_charge_in_c_ / 3600.0f);

          return false;
        case State::CHARGE_REMAINING_SENSOR:
          this->meter_state_ = State::CHARGE_CALCULATED_SENSOR;

          publish_state_(charge_remaining_sensor_, this->current_charge_c_ / 3600.0f);
          return false;
        case State::CHARGE_CALCULATED_SENSOR:
          this->meter_state_ = State::ENERGY_LEVEL_SENSOR;
          if (this->full_charge_calculated_c_.has_value()) {
            ESP_LOGD(TAG, "Charge calculated sensor: %i", this->full_charge_calculated_c_.value());
          } else {
            ESP_LOGD(TAG, "Charge calculated sensor: HAS NO VALUE");
          }
          if (full_charge_calculated_c_.has_value()) {
            publish_state_(this->charge_calculated_sensor_, full_charge_calculated_c_.value() / 3600.0f);
          }
          
          return false;
        case State::ENERGY_LEVEL_SENSOR:
          this->meter_state_ = State::ENERGY_OUT_SENSOR;

          if (energy_level_sensor_ != nullptr) {
            const auto current_energy_level_ = std::min(
                std::max(
                  clamp_map(
                    this->current_energy_j_,
                    0,
                    full_energy_calculated_j_.value_or(full_energy_j_),
                    0,
                    100
                  ),
                  full_discharge_reached_ ? 0 : 1
                ),
                full_charge_reached_ ? 100 : 99
            );
            publish_state_(energy_level_sensor_, current_energy_level_);
          } 
          return false;
        case State::ENERGY_OUT_SENSOR:
          this->meter_state_ = State::ENERGY_IN_SENSOR;
          //ESP_LOGD(TAG, "Energy out sensor: %i", this->cumulative_energy_out_j_);

          publish_state_(energy_out_sensor_, this->cumulative_energy_out_j_ / 3600.0f);

          return false;
        case State::ENERGY_IN_SENSOR:
          this->meter_state_ = State::ENERGY_REMAINING_SENSOR;
          //ESP_LOGD(TAG, "Energy in sensor: %i", this->cumulative_energy_in_j_);
          publish_state_(energy_in_sensor_, this->cumulative_energy_in_j_ / 3600.0f);

          return false;
        case State::ENERGY_REMAINING_SENSOR:
          this->meter_state_ = State::ENERGY_CALCULATED_SENSOR;
          publish_state_(energy_remaining_sensor_, this->current_energy_j_ / 3600.0f);
          return false;
        case State::ENERGY_CALCULATED_SENSOR:
          this->meter_state_ = State::TIME_REMAINING_SENSOR;
           

          // if (this->full_energy_calculated_j_.has_value()) {
          //   ESP_LOGD(TAG, "Energy calculated sensor: %i", this->full_energy_calculated_j_.value());
          // } else {
          //   ESP_LOGD(TAG, "Energy calculated sensor: HAS NO VALUE");
          // }
          if (full_energy_calculated_j_.has_value()) {
            publish_state_(this->energy_calculated_sensor_, full_energy_calculated_j_.value() / 3600.0f);
          }

          return false;
        case State::TIME_REMAINING_SENSOR:
          this->meter_state_ = State::IDLE;

          if (charge_time_remaining_sensor_ != nullptr || discharge_time_remaining_sensor_ != nullptr) {
            const auto avg_energy = this->energy_usage_average_.get();
            const auto avg_energy_usage_minutes = avg_energy * (60000.0f / TIME_REMAINING);

            if (std::abs(avg_energy_usage_minutes) < 0.1) {
              publish_state_(charge_time_remaining_sensor_, NAN);
              publish_state_(discharge_time_remaining_sensor_, NAN);

            } else if (avg_energy_usage_minutes > 0) {
              auto const energy_to_full = full_energy_calculated_j_.value_or(full_energy_j_) - this->current_energy_j_;
              const auto time_remaining = energy_to_full / avg_energy_usage_minutes;
              
              publish_state_(charge_time_remaining_sensor_, std::round(std::min(9999.0f, time_remaining)));

              if (discharge_time_remaining_sensor_ != nullptr) {
                if (!std::isnan(discharge_time_remaining_sensor_->state)) {
                  publish_state_(discharge_time_remaining_sensor_, NAN);
                }
              }
            } else {
              const auto time_remaining = this->current_energy_j_ / -avg_energy_usage_minutes;
    
              if (charge_time_remaining_sensor_ != nullptr && !std::isnan(charge_time_remaining_sensor_->state)) {
                publish_state_(charge_time_remaining_sensor_, NAN);
              }
              publish_state_(discharge_time_remaining_sensor_, std::round(std::min(9999.0f, time_remaining)));
            }

          }
          return false;
        default:
          break;
        }

        return true;
    }

    void CoulombMeter::publish_state_(sensor::Sensor *sensor, float value) {
      if (sensor != nullptr) {
        sensor->publish_state(value);
      }
    }

    void CoulombMeter::dump_config() {

      ESP_LOGCONFIG(TAG, "Coulomb Meter Config: ...");
    }
    float CoulombMeter::get_setup_priority() const { return setup_priority::DATA; }

    float CoulombMeter::get_voltage() { 
      ESP_LOGE(TAG, "get_voltage() not implemented");
      return 0.0f; 
    };
    float CoulombMeter::get_current() {
      ESP_LOGE(TAG, "get_current() not implemented");
      return 0.0f; 
    };
    int64_t CoulombMeter::get_charge_c() {
      ESP_LOGE(TAG, "get_charge_c() not implemented");
      return 0;
    };
    int64_t CoulombMeter::get_energy_j() {
      ESP_LOGE(TAG, "get_energy_j() not implemented");
      return 0;
    };

  } // namespace coulomb_meter
} // namespace esphome
