

#include "coulomb_meter.h"

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
      pref_current_charge_c_ = global_preferences->make_preference<int32_t>(fnv1_hash("CoulombMeter_current_charge_c"), true);
      pref_current_energy_j_ = global_preferences->make_preference<int32_t>(fnv1_hash("CoulombMeter_current_energy_j"), true);
      pref_full_charge_calculated_c_ = global_preferences->make_preference<int32_t>(fnv1_hash("CoulombMeter_full_charge_calculated_c"), true);
      pref_full_energy_calculated_j_ = global_preferences->make_preference<int32_t>(fnv1_hash("CoulombMeter_full_energy_calculated_j"), true);
      
      int32_t charge_calculated_c_;
      if (pref_full_charge_calculated_c_.load(&charge_calculated_c_)) {
        ESP_LOGD(TAG, "Loaded full charge from flash: %i", charge_calculated_c_);
        full_charge_calculated_c_ = charge_calculated_c_;
      } else {
        ESP_LOGD(TAG, "Failed to load full charge, use default");
      };
      int32_t energy_calculated_j_;
      if (pref_full_energy_calculated_j_.load(&energy_calculated_j_)) {
        ESP_LOGD(TAG, "Loaded full energy from flash: %i", energy_calculated_j_);
        full_energy_calculated_j_ = energy_calculated_j_;
      } else {
        ESP_LOGD(TAG, "Failed to load full energy, use default");
      };

      this->set_interval("saveCounters", 15 * 60 * 1000, [this]() { 
        // store energy if significant change every 15min (to reduce flash wear)
        int32_t stored_energy_value;

        if (pref_current_energy_j_.load(&stored_energy_value)) {
          const auto current_stored_energy_level_ = clamp_map(
            stored_energy_value,
            0,
            full_energy_calculated_j_.value_or(full_energy_j_),
            0,
            100
          );
          const auto current_energy_level_ = clamp_map(
            current_energy_j_,
            0,
            full_energy_calculated_j_.value_or(full_energy_j_),
            0,
            100
          );
          if (current_energy_level_ != current_stored_energy_level_) {
            ESP_LOGD(TAG, "Saving current energy: %i", current_energy_j_);
            pref_current_energy_j_.save(&current_energy_j_);
          } else {
            ESP_LOGD(TAG, "Current energy not changed enought: %i (in flash %i)", current_energy_j_, stored_energy_value);
          }
        } else {
          ESP_LOGD(TAG, "Saving current energy: %i", current_energy_j_);
          pref_current_energy_j_.save(&current_energy_j_);
        }
        // store charge if significant change every 15min
        int32_t stored_charge_value;

        if (pref_current_charge_c_.load(&stored_charge_value)) {
          const auto current_stored_charge_level_ = clamp_map(
            stored_charge_value,
            0,
            full_charge_calculated_c_.value_or(full_capacity_c_),
            0,
            100
          );
          const auto current_charge_level_ = clamp_map(
            current_charge_c_,
            0,
            full_charge_calculated_c_.value_or(full_capacity_c_),
            0,
            100
          );
          if (current_charge_level_ != current_stored_charge_level_) {
            ESP_LOGD(TAG, "Saving current charge: %i", current_charge_c_);
            pref_current_charge_c_.save(&current_charge_c_);
          } else {
            ESP_LOGD(TAG, "Current charge not changed enought: %i (in flash %i)", current_charge_c_, stored_charge_value);
          }
        } else {
          ESP_LOGD(TAG, "Saving current charge: %i", current_charge_c_);
          pref_current_charge_c_.save(&current_charge_c_);
        }

      });
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
        int32_t charge;

        if (pref_current_charge_c_.load(&charge)) {
          ESP_LOGD(TAG, "Loaded current charge from flash: %i", charge);
          current_charge_c_ = charge;
        } else {
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

        int32_t energy;

        if (pref_current_energy_j_.load(&energy)) {
          ESP_LOGD(TAG, "Loaded current energy from flasg: %i", energy);
          current_energy_j_ = energy;
        } else {
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

      if (voltage >= fully_charge_voltage_ && (fully_charge_current_.has_value() ? this->get_current() <= fully_charge_current_.value() : true)) {
        fully_charge_time_.start([this]() {
          full_charge_reached_ = true;
          this->current_charge_c_ = full_charge_calculated_c_.value_or(full_capacity_c_);
          cumulative_at_fill_charge_c_ = this->cumulative_charge_in_c_;
          cumulative_at_full_discharge_c_ = this->cumulative_charge_out_c_; 
          cumulative_at_fill_charge_j_ = this->cumulative_energy_in_j_;
          cumulative_at_full_discharge_j_ = this->cumulative_energy_out_j_;
        });
      } else {
        fully_charge_time_.stop();
      }

      if (voltage <= fully_discharge_voltage_v_) {
          fully_discharge_timer_.start([this]() {
          full_discharge_reached_ = true;
          this->current_charge_c_ = 0;
          if (cumulative_at_fill_charge_c_.has_value() && cumulative_at_full_discharge_c_.has_value()) {
            const auto charge_delta = cumulative_charge_in_c_ - cumulative_at_fill_charge_c_.value();
            const auto discharge_delta = cumulative_charge_out_c_ - cumulative_at_full_discharge_c_.value();
            const auto capacity = charge_delta + discharge_delta;

            ESP_LOGD(TAG, "Capacity calculated: %i", capacity);
            ESP_LOGD(TAG, "Charge delta: %i", charge_delta);
            ESP_LOGD(TAG, "Discharge delta: %i", discharge_delta);
            if (capacity > 0) {
              full_charge_calculated_c_ = capacity;
              int32_t stored_capacity;
              if (pref_full_charge_calculated_c_.load(&stored_capacity)) {
                if (stored_capacity != capacity) {
                  ESP_LOGD(TAG, "Saving full charge: %i", capacity);
                  pref_full_charge_calculated_c_.save(&capacity);
                }
              } else {
                pref_full_charge_calculated_c_.save(&capacity);
              }
              ESP_LOGD(TAG, "Capacity calculated: %i", capacity);
            } else {
              ESP_LOGW(TAG, "Capacity invalid: %i", capacity);
            }
          }
          if (cumulative_at_fill_charge_j_.has_value() && cumulative_at_full_discharge_j_.has_value()) {
            const auto energy_delta = cumulative_energy_in_j_ - cumulative_at_fill_charge_j_.value();
            const auto discharge_delta = cumulative_energy_out_j_ - cumulative_at_full_discharge_j_.value();
            const auto energy_capacity = energy_delta + discharge_delta;

            ESP_LOGD(TAG, "Energy capacity calculated: %i", energy_capacity);
            ESP_LOGD(TAG, "Energy delta: %i", energy_delta);
            ESP_LOGD(TAG, "Discharge delta: %i", discharge_delta);
            if (energy_capacity > 0) {
              full_energy_calculated_j_ = energy_capacity;
              int32_t stored_energy;
              if (pref_full_energy_calculated_j_.load(&stored_energy)) {
                if (stored_energy != energy_capacity) {
                  ESP_LOGD(TAG, "Saving full energy: %i", energy_capacity);
                  pref_full_energy_calculated_j_.save(&energy_capacity);
                }
              } else {
                pref_full_energy_calculated_j_.save(&energy_capacity);
              }
              ESP_LOGD(TAG, "Energy capacity calculated: %i", energy_capacity);
            } else {
              ESP_LOGW(TAG, "Energy capacity invalid: %i", energy_capacity);
            }
          }
        });
      } else {
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
          if (this->charge_calculated_sensor_ != nullptr && full_charge_calculated_c_.has_value()) {

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
          if (this->energy_calculated_sensor_ != nullptr && full_energy_calculated_j_.has_value()) {
            publish_state_(this->energy_calculated_sensor_, full_energy_calculated_j_.value() / 3600.0f);
          }

          return false;
        case State::TIME_REMAINING_SENSOR:
          this->meter_state_ = State::IDLE;

          if (charge_time_remaining_sensor_ != nullptr || discharge_time_remaining_sensor_ != nullptr) {
            const auto avg_energy = this->energy_usage_average_.get();
            const auto avg_energy_usage_minutes = avg_energy * (60000.0f / TIME_REMAINING);

            if (avg_energy_usage_minutes == 0) {
              publish_state_(charge_time_remaining_sensor_, NAN);
              publish_state_(discharge_time_remaining_sensor_, NAN);

            } else if (avg_energy_usage_minutes > 0) {
              auto const energy_to_full = full_energy_calculated_j_.value_or(full_energy_j_) - this->current_energy_j_;
              const auto time_remaining = energy_to_full / avg_energy_usage_minutes;
              
              publish_state_(charge_time_remaining_sensor_, std::round(std::min(9999.0f, time_remaining)));

              if (discharge_time_remaining_sensor_ != nullptr && !std::isnan(discharge_time_remaining_sensor_->state)) {
                publish_state_(discharge_time_remaining_sensor_, NAN);
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
  } // namespace coulomb_meter
} // namespace esphome
