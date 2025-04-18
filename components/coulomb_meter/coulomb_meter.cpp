

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
        this->energy_usage_average_.setup(6);
        this->set_interval("updateTimeRemaining", TIME_REMAINING, [this]() { 
          const auto delta_energy = this->current_energy_j_ - this->prev_time_energy_j_;
          this->prev_time_energy_j_ = this->current_energy_j_;

          this->energy_usage_average_.add(delta_energy);
        });
      }
    }

    void CoulombMeter::update() {
      if (this->meter_state_ == State::IDLE) {
        this->meter_state_ = State::START;
      }
    }

    void CoulombMeter::updateState() {
      // auto test = global_preferences->make_preference<int64_t>(123456);
      if (this->meter_state_ == State::NOT_INITIALIZED) {
        return;
      }
      const auto voltage = this->getVoltage();

      if (this->meter_state_ == State::SETUP) {
        // use initial charge based on voltage
        current_charge_c_ = clamp_map(
          voltage * 1000,
          fully_discharge_voltage_v_ * 1000,
          fully_charge_voltage_ * 1000,
          0,
          full_charge_calculated_c_.value_or(full_capacity_c_)
        );

        current_energy_j_ = clamp_map(
            voltage * 1000,
            fully_discharge_voltage_v_ * 1000,
            fully_charge_voltage_ * 1000,
            0,
            full_energy_calculated_j_.value_or(full_energy_j_)
        );
        prev_time_energy_j_ = current_energy_j_;
        this->meter_state_ = State::IDLE;
      }

      const auto charge = this->getCharge_c();

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
        if (current_charge_level_ < 99) {
          full_charge_reached_ = false;
        } else if (current_charge_level_ > 1) {
          full_discharge_reached_ = false;
        }
      }

      const auto energy_j = this->getEnergy_j();

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

      if (voltage >= fully_charge_voltage_ && (fully_charge_current_.has_value() ? this->getCurrent() <= fully_charge_current_.value() : true)) {
        fully_charge_time_.start([this]() {
          full_charge_reached_ = true;
          this->current_charge_c_ = full_charge_calculated_c_.value_or(full_capacity_c_);
          cumulative_at_fill_charge_c_ = this->cumulative_charge_in_c_;
          cumulative_at_full_discharge_c_ = this->cumulative_charge_out_c_; 
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
            ESP_LOGD(TAG, "Capacity calculated: %i", capacity);
          } else {
            ESP_LOGW(TAG, "Capacity invalid: %i", capacity);
          }
        } });
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

          charge_level_sensor_->publish_state(current_charge_level_);
          return false;
        } else {
          return updateSensors();
        }
        break;
      case State::CHARGE_OUT_SENSOR:
        this->meter_state_ = State::CHARGE_IN_SENSOR;
        if (charge_out_sensor_ != nullptr) {
          charge_out_sensor_->publish_state(this->cumulative_charge_out_c_ / 3600.0f);
          return false;
        } else {
          return updateSensors();
        }
        break;
      case State::CHARGE_IN_SENSOR:
        this->meter_state_ = State::CHARGE_REMAINING_SENSOR;

        if (charge_in_sensor_ != nullptr) {
          charge_in_sensor_->publish_state(this->cumulative_charge_in_c_ / 3600.0f);
          return false;
        } else {
          return updateSensors();
        }
        break;
      case State::CHARGE_REMAINING_SENSOR:
        this->meter_state_ = State::ENERGY_LEVEL_SENSOR;

        if (charge_remaining_sensor_ != nullptr) {
          charge_remaining_sensor_->publish_state(this->current_charge_c_ / 3600.0f);
          return false;
        } else {
          return updateSensors();
        }

        break;
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
    
          energy_level_sensor_->publish_state(current_energy_level_);
          return false;
        } else {
          return updateSensors();
        }

        break;
      case State::ENERGY_OUT_SENSOR:
        this->meter_state_ = State::ENERGY_IN_SENSOR;
        ESP_LOGD(TAG, "Energy out sensor: %i", this->cumulative_energy_out_j_);

        if (energy_out_sensor_ != nullptr) {
          energy_out_sensor_->publish_state(this->cumulative_energy_out_j_ / 3600.0f);
          return false;
        } else {
          return updateSensors();
        }

        break;
      case State::ENERGY_IN_SENSOR:
        this->meter_state_ = State::ENERGY_REMAINING_SENSOR;
        ESP_LOGD(TAG, "Energy in sensor: %i", this->cumulative_energy_in_j_);
        if (energy_in_sensor_ != nullptr) {
          energy_in_sensor_->publish_state(this->cumulative_energy_in_j_ / 3600.0f);
          return false;
        } else {
          return updateSensors();
        }

        break;
      case State::ENERGY_REMAINING_SENSOR:
        this->meter_state_ = State::TIME_REMAINING_SENSOR;

        if (energy_remaining_sensor_ != nullptr) {
          energy_remaining_sensor_->publish_state(this->current_energy_j_ / 3600.0f);
          return false;
        } else {
          return updateSensors();
        }

        break;
      case State::TIME_REMAINING_SENSOR:
        this->meter_state_ = State::IDLE;

        if (charge_time_remaining_sensor_ != nullptr || discharge_time_remaining_sensor_ != nullptr) {
          const auto avg_energy = this->energy_usage_average_.get();
          const auto avg_energy_usage_minutes = avg_energy * (60000.0f / TIME_REMAINING);

          if (avg_energy_usage_minutes == 0) {
            if (charge_time_remaining_sensor_ != nullptr) {
              charge_time_remaining_sensor_->publish_state(0);
            }
            if (discharge_time_remaining_sensor_ != nullptr) {
              discharge_time_remaining_sensor_->publish_state(0);
            }
          } else if (avg_energy_usage_minutes > 0) {
            auto const energy_to_full = full_energy_calculated_j_.value_or(full_energy_j_) - this->current_energy_j_;
            const auto time_remaining = energy_to_full / avg_energy_usage_minutes;
            

            if (charge_time_remaining_sensor_ != nullptr) {
              charge_time_remaining_sensor_->publish_state(time_remaining);
            }
            if (discharge_time_remaining_sensor_ != nullptr) {
              discharge_time_remaining_sensor_->publish_state(0);
            }
          } else {
            const auto time_remaining = this->current_energy_j_ / avg_energy_usage_minutes;
  
            if (charge_time_remaining_sensor_ != nullptr) {
              charge_time_remaining_sensor_->publish_state(0);
            }
            if (discharge_time_remaining_sensor_ != nullptr) {
              discharge_time_remaining_sensor_->publish_state(-time_remaining);
            }
          }

          return false;
        } else {
          return updateSensors();
        }

        break;
      default:
        break;
      }

      return true;
    }
  } // namespace coulomb_meter
} // namespace esphome
