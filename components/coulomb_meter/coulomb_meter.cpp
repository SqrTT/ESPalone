#include "coulomb_meter.h"

// .platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-addr2line -pfiaC -e esphome_config/.esphome/build/ina226coulomb/.pioenvs/ina226coulomb/firmware.elf  0x4023bf0b
namespace esphome {
  namespace coulomb_meter {

    static const char *const TAG = "CoulombMeter";
    static const unsigned int TIME_REMAINING = 30000; // ms
    static const auto SENSORS_COUNT = 13;

    int32_t clamp_map(int32_t x, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max)
    {
      // ESP_LOGV(TAG, "Clamp map: %i %i %i %i %i", x, in_min, in_max, out_min, out_max);
      if (x <= in_min)
        return out_min;
      if (x >= in_max)
        return out_max;

      // Prevent division by zero
      if (in_max == in_min) {
          #ifdef ESPHOME_LOG_HAS_ERROR
            ESP_LOGE(TAG, "clamp_map: Division by zero detected (in_max == in_min)");
          #endif
          return out_min; // Return a safe default value
      }
      return (int32_t) ((int64_t)(x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min);
    }

    void CoulombMeter::setup() {
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
        #ifdef ESPHOME_LOG_HAS_DEBUG
          ESP_LOGD(TAG, "Loaded full charge from flash: %i", charge_calculated_c_);
        #endif
        full_charge_calculated_c_ = charge_calculated_c_;
      } else {
        #ifdef ESPHOME_LOG_HAS_DEBUG
          ESP_LOGD(TAG, "Failed to load full charge, use default");
        #endif  
      };
      int32_t energy_calculated_j_;
      if (flash_full_energy_calculated_j_.load(&energy_calculated_j_)) {
        #ifdef ESPHOME_LOG_HAS_DEBUG
          ESP_LOGD(TAG, "Loaded full energy from flash: %i", energy_calculated_j_);
        #endif
        full_energy_calculated_j_ = energy_calculated_j_;
      } else {
        #ifdef ESPHOME_LOG_HAS_DEBUG
          ESP_LOGD(TAG, "Failed to load full energy, use default");
        #endif
      };

      int32_t charge;
      if (rtc_current_charge_c_.load(&charge)) {
        #ifdef ESPHOME_LOG_HAS_DEBUG
          ESP_LOGD(TAG, "Loaded current charge from rtc: %i", charge);
        #endif
        current_charge_c_ = charge;
      }

      int32_t energy;
      if (rtc_current_energy_j_.load(&energy)) {
        #ifdef ESPHOME_LOG_HAS_DEBUG
          ESP_LOGD(TAG, "Loaded current energy from rtc: %i", energy);
        #endif
        current_energy_j_ = energy;
      }

      this->prev_time_energy_j_ = this->current_energy_j_;
      this->previous_charge_c_ = this->get_charge_c();
      this->previous_energy_j_ = this->get_energy_j();

      this->set_interval("updateStatus", 1000, [this]() { updateState(); });

      this->set_interval("reportSensors", this->get_update_interval() / SENSORS_COUNT, [this] { reportSensors(); });
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

    void CoulombMeter::updateState() {
      const auto voltage = this->get_voltage();
      const auto charge = this->get_charge_c();

      const auto delta_charge = charge - this->previous_charge_c_;
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
          #ifdef ESPHOME_LOG_HAS_DEBUG
            ESP_LOGD(TAG, "Full charge reached: %i", this->current_charge_c_);
          #endif
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
            if (discharge_delta > charge_delta) {
              const auto capacity = (int32_t) (discharge_delta - charge_delta);
              #ifdef ESPHOME_LOG_HAS_DEBUG
                ESP_LOGD(TAG, "Capacity calculated: %d, charge_delta: %llu, discharge_delta: %llu", capacity, charge_delta, discharge_delta);
              #endif

              full_charge_calculated_c_ = capacity;
              int32_t stored_capacity = 0;
              if (full_charge_calculated_c_.has_value() && flash_full_charge_calculated_c_.load(&stored_capacity)) {
                if (stored_capacity != full_charge_calculated_c_.value()) {
                  #ifdef ESPHOME_LOG_HAS_DEBUG
                    ESP_LOGD(TAG, "Full charge calculated: %i", full_charge_calculated_c_.value());
                  #endif
                  
                  const auto value_to_save = full_charge_calculated_c_.value();
                  flash_full_charge_calculated_c_.save(&value_to_save);
                }
              } else if (full_charge_calculated_c_.has_value()) {
                const auto value_to_save = full_charge_calculated_c_.value();
                flash_full_charge_calculated_c_.save(&value_to_save);
              }
            } else {
              #ifdef ESPHOME_LOG_HAS_WARN
                ESP_LOGW(TAG, "Capacity invalid: discharge_delta (%llu) <= charge_delta (%llu)", discharge_delta, charge_delta);
              #endif
            }
          }


          // calculate energy based on energy
          uint64_t cumulative_at_full_in_j_ = 0;
          uint64_t cumulative_at_full_out_j_ = 0;

          if (
            rtc_cumulative_at_full_in_j_.load(&cumulative_at_full_in_j_) &&
            rtc_cumulative_at_full_out_j_.load(&cumulative_at_full_out_j_)
          ) {
            const auto energy_in_delta = cumulative_energy_in_j_ - cumulative_at_full_in_j_;
            const auto energy_out_delta = cumulative_energy_out_j_ - cumulative_at_full_out_j_;

            if (energy_out_delta > energy_in_delta) {
              const auto energy_capacity = (int32_t) (energy_out_delta - energy_in_delta);

              #ifdef ESPHOME_LOG_HAS_DEBUG
                ESP_LOGD(TAG, "Energy capacity calculated: %d, energy_delta: %llu, discharge_delta: %llu", energy_capacity, energy_in_delta, energy_out_delta);
              #endif

              full_energy_calculated_j_ = energy_capacity;
              int32_t stored_energy = 0;
              if (full_energy_calculated_j_.has_value() && flash_full_energy_calculated_j_.load(&stored_energy)) {
                if (stored_energy != full_energy_calculated_j_.value()) {
                  #ifdef ESPHOME_LOG_HAS_DEBUG
                    ESP_LOGD(TAG, "Saving full energy: %i", full_energy_calculated_j_.value());
                  #endif
                  const auto value_to_save = full_energy_calculated_j_.value();
                  flash_full_energy_calculated_j_.save(&value_to_save);
                }
              } else if (full_energy_calculated_j_.has_value()) {
                const auto value_to_save = full_energy_calculated_j_.value();
                flash_full_energy_calculated_j_.save(&value_to_save);
              }
            } else {
              #ifdef ESPHOME_LOG_HAS_WARN
                ESP_LOGW(TAG, "Energy capacity invalid: energy_out_delta (%llu) <= energy_in_delta (%llu)", energy_out_delta, energy_in_delta);
              #endif
            }
          }
        });
      } else {
        // if voltage is above fully discharge voltage, stop timer
        fully_discharge_timer_.stop();
      }
    }

    void CoulombMeter::reportSensors() {
      
      switch (report_count_ % SENSORS_COUNT) {
        case 0:
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
          break;
        case 1:
          if (charge_out_sensor_ != nullptr) {
            publish_state_(charge_out_sensor_, this->cumulative_charge_out_c_ / 3600.0f);
          }
          break;
        case 2:
          if (charge_in_sensor_ != nullptr) {
            publish_state_(charge_in_sensor_, this->cumulative_charge_in_c_ / 3600.0f);
          }
          break;
        case 3:
          if (charge_remaining_sensor_ != nullptr) {
            publish_state_(charge_remaining_sensor_, this->current_charge_c_ / 3600.0f);
          }
          break;
        case 4:
          if (charge_calculated_sensor_ != nullptr && full_charge_calculated_c_.has_value()) {
            publish_state_(charge_calculated_sensor_, full_charge_calculated_c_.value() / 3600.0f);
          }
          break;
        case 5:
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
          break;
        case 6:
          if (energy_calculated_sensor_ != nullptr && full_energy_calculated_j_.has_value()) {
            publish_state_(energy_calculated_sensor_, full_energy_calculated_j_.value() / 3600.0f);
          }
          break;
        case 7:
          if (energy_remaining_sensor_ != nullptr) {
            publish_state_(energy_remaining_sensor_, this->current_energy_j_ / 3600.0f);
          }
          break;
        case 8:
          if (energy_out_sensor_ != nullptr) {
            publish_state_(energy_out_sensor_, this->cumulative_energy_out_j_ / 3600.0f);
          }
          break;
        case 9:
          if (energy_in_sensor_ != nullptr) {
            publish_state_(energy_in_sensor_, this->cumulative_energy_in_j_ / 3600.0f);
          }
          break;
        case 10:
          break;
        case 11:
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
        break;
        case 12:
          this->storeCounters();
          break;
        default:
          break;
      }
      report_count_++;
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
      #ifdef ESPHOME_LOG_HAS_ERROR
        ESP_LOGE(TAG, "get_voltage() not implemented");
      #endif
      return 0.0f; 
    };
    float CoulombMeter::get_current() {
      #ifdef ESPHOME_LOG_HAS_ERROR
      ESP_LOGE(TAG, "get_current() not implemented");
      #endif
      return 0.0f; 
    };
    int64_t CoulombMeter::get_charge_c() {
      #ifdef ESPHOME_LOG_HAS_ERROR
        ESP_LOGE(TAG, "get_charge_c() not implemented");
      #endif
      return 0;
    };
    int64_t CoulombMeter::get_energy_j() {
      #ifdef ESPHOME_LOG_HAS_ERROR
        ESP_LOGE(TAG, "get_energy_j() not implemented");
      #endif
      return 0;
    };

  } // namespace coulomb_meter
} // namespace esphome
