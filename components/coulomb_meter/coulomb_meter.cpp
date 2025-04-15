

#include "coulomb_meter.h"



namespace esphome {
namespace coulomb_meter {

  static const char *const TAG = "CoulombMeter";


  int32_t clamp_map(int32_t x, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max) {
    // ESP_LOGV(TAG, "Clamp map: %i %i %i %i %i", x, in_min, in_max, out_min, out_max);
    if (x <= in_min) return out_min;
    if (x >= in_max) return out_max;
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
  }

  void CoulombMeter::setup() {
    this->set_interval("updateStatus", 1000, [this]() {
      this->updateState();
    });    
  }

  void CoulombMeter::updateState() {
    //auto test = global_preferences->make_preference<int64_t>(123456); 
    if (this->meter_state_ == State::NOT_INITIALIZED) {
      return;
    }
    auto voltage = this->getVoltage();

    if (this->meter_state_ == State::IDLE) {
      // use initial charge based on voltage
      current_charge_c_ = clamp_map(
        voltage * 1000,
        fully_discharge_voltage_v_ * 1000,
        fully_charge_voltage_ * 1000,
        0,
        full_capacity_calculated_c_.value_or(full_capacity_c_)
      );
      this->meter_state_ = State::DATA_CALCULATE_CHARGE;
    }

        

    
    auto charge = this->getCharge_c();

    int32_t delta_charge = charge - this->previous_charge_c_;
    this->previous_charge_c_ = charge;

    if (delta_charge > 0) {
      this->cumulative_charge_c_ += delta_charge;
    } else if (delta_charge < 0) {
      this->cumulative_discharge_c_ += -delta_charge;
    }
    current_charge_c_ += delta_charge;

    if (current_charge_c_ < 0) {
      current_charge_c_ = 0;
    }
    if (current_charge_c_ > full_capacity_calculated_c_.value_or(full_capacity_c_)) {
      current_charge_c_ = full_capacity_calculated_c_.value_or(full_capacity_c_);
    }


    current_soc_ = clamp_map(
        this->current_charge_c_,
        0,
        full_capacity_calculated_c_.value_or(full_capacity_c_),
        0,
        100
      );
   
  
    if (full_charge_reached_ && current_soc_ < 100) {
      full_charge_reached_ = false;
    } else if (!full_charge_reached_ && current_soc_ == 100) {
      current_soc_ = 99;
    }
    if (full_discharge_reached_ && current_soc_ > 0) {
      full_discharge_reached_ = false;
    } else if (!full_discharge_reached_ && current_soc_ == 0) {
      current_soc_ = 1;
    }

    if (voltage >= fully_charge_voltage_ && (fully_charge_current_.has_value() ? this->getCurrent() <= fully_charge_current_.value() : true )) {
      fully_charge_time_.start([this]() {
        full_charge_reached_ = true;
        this->current_charge_c_ = full_capacity_calculated_c_.value_or(full_capacity_c_);
        cumulative_at_fill_charge_c_ = this->cumulative_charge_c_;
        cumulative_at_full_discharge_c_ = this->cumulative_discharge_c_;
      });
    } else {
      fully_charge_time_.stop();
    }
    if (voltage <= fully_discharge_voltage_v_) {
      fully_discharge_timer_.start([this]() {
        full_discharge_reached_ = true;
        this->current_charge_c_ = 0;
        if (cumulative_at_fill_charge_c_.has_value() && cumulative_at_full_discharge_c_.has_value()) {
          auto charge_delta = cumulative_charge_c_ - cumulative_at_fill_charge_c_.value();
          auto discharge_delta = cumulative_discharge_c_ - cumulative_at_full_discharge_c_.value();
          auto capacity = charge_delta + discharge_delta;
          ESP_LOGD(TAG, "Capacity calculated: %i", capacity);
          ESP_LOGD(TAG, "Charge delta: %i", charge_delta);
          ESP_LOGD(TAG, "Discharge delta: %i", discharge_delta);
          if (capacity > 0) {
            full_capacity_calculated_c_ = capacity;
            ESP_LOGD(TAG, "Capacity calculated: %i", capacity);
          } else {
            ESP_LOGW(TAG, "Capacity invalid: %i", capacity);
          }
        }
      });
    } else {
      fully_discharge_timer_.stop();
    }
  }

  void CoulombMeter::update() {
    if (this->meter_state_ == State::NOT_INITIALIZED) {
      this->meter_state_ = State::IDLE;
      return;
    }
    if (charge_sensor_ != nullptr) {
      charge_sensor_->publish_state(this->cumulative_charge_c_ / 3600.0f);
    }
    if (discharge_sensor_ != nullptr) {
      discharge_sensor_->publish_state(this->cumulative_discharge_c_ / 3600.0f);
    }
    if (soc_target_sensor_ != nullptr) {
      soc_target_sensor_->publish_state(current_soc_);
    }
    if (remaining_sensor_ != nullptr) {
      remaining_sensor_->publish_state(this->current_charge_c_ / 3600.0f);
    }

    // if (full_capacity_calculated_c_.has_value()) {
    //   ESP_LOGV(TAG, "Capacity: %i", full_capacity_calculated_c_.value());
    // }
  }
}  // namespace coulomb_meter
}  // namespace esphome
