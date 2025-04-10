#include "battery_charger.h"


namespace esphome {
namespace battery_charger {

static const char *const TAG = "BatteryCharger";

float ChargerComponent::get_setup_priority() const { return setup_priority::DATA; }


void ChargerComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "ChargerComponent:");

  // Output voltage settings
  ESP_LOGCONFIG(TAG, "  Float Voltage: %0.2f V", this->float_voltage_.value_or(-1));
  ESP_LOGCONFIG(TAG, "  Absorption Voltage: %0.2f V", this->absorption_voltage_v_.value_or(-1));
  ESP_LOGCONFIG(TAG, "  Equaization Voltage: %0.2f V", this->equaization_voltage_v_.value_or(-1));
  ESP_LOGCONFIG(TAG, "  Absorption Restart Voltage: %0.2f V", this->absorption_restart_voltage_v_.value_or(-1));
  
  // Output current and voltage readings
  ESP_LOGCONFIG(TAG, "  Current Voltage: %0.2f V", this->last_voltage_);
  ESP_LOGCONFIG(TAG, "  Current Current: %0.2f A", this->last_current_.value());

  // Output timer and delay settings
  ESP_LOGCONFIG(TAG, "  Absorption Timer: %d seconds", this->absorption_timer_.time_s);
  ESP_LOGCONFIG(TAG, "  Voltage Auto Recovery Delay: %d seconds", this->voltage_auto_recovery_delay_timer_.time_s);
  ESP_LOGCONFIG(TAG, "  Absorption Restart Time: %d seconds", this->absorption_restart_timer_.time_s);
  ESP_LOGCONFIG(TAG, "  Equaization Timeout: %d seconds", this->equaization_timeout_timer_.time_s);
  ESP_LOGCONFIG(TAG, "  Equaization Interval: %d seconds", this->equaization_interval_timer_.time_s);
  ESP_LOGCONFIG(TAG, "  Absorption Low Voltage Delay: %d seconds", this->absorption_low_voltage_timer_.time_s);

  // Output current charge state
  const char* charge_state_str = "UNKNOWN";
  switch (this->charge_state_) {
      case INITIAL: charge_state_str = "INITIAL"; break;
      case BEFORE_ABSORPTION: charge_state_str = "BEFORE_ABSORPTION"; break;
      case ABSORPTION: charge_state_str = "ABSORPTION"; break;
      case BEFORE_FLOAT: charge_state_str = "BEFORE_FLOAT"; break;
      case FLOAT: charge_state_str = "FLOAT"; break;
      case BEFORE_EQUAIZATION: charge_state_str = "BEFORE_EQUAIZATION"; break;
      case EQUAIZATION: charge_state_str = "EQUAIZATION"; break;
      case ERROR: charge_state_str = "ERROR"; break;
      case BEFORE_ERROR: charge_state_str = "BEFORE_ERROR"; break;
      default: charge_state_str = "INVALID_STATE"; break;
  }
  ESP_LOGCONFIG(TAG, "  Current Charge State: %s", charge_state_str);

  // Output additional settings if available
  if (this->voltage_target_sensor_ != nullptr) {
      ESP_LOGCONFIG(TAG, "  Voltage Target Sensor is set.");
  } else {
      ESP_LOGCONFIG(TAG, "  Voltage Target Sensor is NOT set.");
  }

  if (this->current_sensor_ != nullptr) {
      ESP_LOGCONFIG(TAG, "  Current Sensor is set.");
  } else {
      ESP_LOGCONFIG(TAG, "  Current Sensor is NOT set.");
  }

  if (this->charge_state_sensor_ != nullptr) {
      ESP_LOGCONFIG(TAG, "  Charge State Sensor is set.");
  } else {
      ESP_LOGCONFIG(TAG, "  Charge State Sensor is NOT set.");
  }

  if (this->voltage_sensor_ != nullptr) {
      ESP_LOGCONFIG(TAG, "  Voltage Sensor is set.");
  } else {
      ESP_LOGCONFIG(TAG, "  Voltage Sensor is NOT set.");
  }

  // Output additional settings for absorption current threshold
  ESP_LOGCONFIG(TAG, "  Absorption Current Threshold: %0.2f A", this->absorption_current_a_.value_or(-1));
  ESP_LOGCONFIG(TAG, "  Max Voltage: %0.2f V", this->max_voltage_.value());
  ESP_LOGCONFIG(TAG, "  Min Voltage: %0.2f V", this->min_voltage_.value_or(-1));
}


void ChargerComponent::setup() {
    ESP_LOGCONFIG(TAG, "Setting up ChargerComponent...");

    if (this->voltage_sensor_ == nullptr) {
      ESP_LOGE(TAG, "Missing required voltage sensor");
      this->mark_failed();
      return;
    }

    if (!this->float_voltage_.has_value()) {
      ESP_LOGE(TAG, "Missing required float voltage");
      this->mark_failed();
      return;
    }
    this->set_timeout("NO_VOLTAGE_UPDATE_FOR_LONG_TIME", 5 * 60 * 1000, [this]() {
      this->status_set_error("No voltage updates, is sensor working?");
      this->charge_state_ = BEFORE_ERROR;
      this->updateState();
    });
    this->voltage_sensor_->add_on_state_callback([this](float voltage){
      ESP_LOGV(TAG, "Volatge: %.2f V", voltage);
      this->last_voltage_ = voltage;
      this->set_timeout("NO_VOLTAGE_UPDATE_FOR_LONG_TIME", 5 * 60 * 1000, [this]() {
        this->status_set_error("No voltage update for long time, is sensor working?");
        this->charge_state_ = BEFORE_ERROR;
        this->updateState();
      });
      
      this->updateState();
    });

    if (this->current_sensor_ != nullptr) {
      this->current_sensor_->add_on_state_callback([this](float current){
        ESP_LOGV(TAG, "Current: %.2f A", current);
        this->last_current_ = current;
        this->updateState();
      });
    }

    this->status_clear_warning();
}

void ChargerComponent::updateState() {

  if (this->charge_state_ != ERROR && this->charge_state_ != BEFORE_ERROR) {
    if (this->max_voltage_.has_value() && this->last_voltage_  > this->max_voltage_.value_or(-1)) {
      ESP_LOGE(TAG, "ERROR: Voltage '%.2fV' is over max voltage '%.2fV'", this->last_voltage_ , this->max_voltage_.value_or(-1));
      this->charge_state_ = BEFORE_ERROR;
    } else if (this->min_voltage_.has_value()  && this->last_voltage_  < this->min_voltage_.value_or(-1)) {
      ESP_LOGE(TAG, "ERROR: Voltage '%.2fV' is under min voltage '%.2fV'", this->last_voltage_ , this->min_voltage_.value_or(-1));
      this->charge_state_ = BEFORE_ERROR;
    }
  }

  switch(this->charge_state_) {
    case INITIAL:
      ESP_LOGD(TAG, "Charge status INITIAL");
      if (this->absorption_voltage_v_.has_value()) {
        this->call_update_state_later(BEFORE_ABSORPTION);
      } else {
        this->call_update_state_later(BEFORE_FLOAT);
      }
      if (this->equaization_voltage_v_.has_value()) {
        this->equaization_interval_timer_.start([this]() {
          this->call_update_state_later(BEFORE_EQUAIZATION);
        });
      }
    break;
    case BEFORE_ABSORPTION:
      ESP_LOGD(TAG, "Charge status becoming ABSORPTION");
      if (this->voltage_target_sensor_ != nullptr) {
        this->voltage_target_sensor_->publish_state(this->absorption_voltage_v_.value_or(0));
      }
      #ifdef USE_TEXT_SENSOR
      if (this->charge_state_sensor_ != nullptr) {
        this->charge_state_sensor_->publish_state("ABSORPTION");
      }
      #endif
      this->absorption_restart_timer_.stop();
      this->absorption_timer_.stop();
      this->absorption_low_voltage_timer_.stop();

      this->call_update_state_later(ABSORPTION);
    break;
    case ABSORPTION:
      ESP_LOGV(TAG, "Charge status ABSORPTION");
      if (this->last_voltage_ >= this->absorption_voltage_v_.value_or(-1)) {
        ESP_LOGV(TAG, "Voltage has been reached absorption level: %.02f of %.02f", this->last_voltage_, this->absorption_voltage_v_.value_or(-1));
        if (this->absorption_current_a_.has_value() && this->last_current_.has_value() && this->last_current_.value() > this->absorption_current_a_.value()) {
          ESP_LOGV(TAG, "Cancel absorption timer: due to current is above required level: %.2f of %.2f", this->last_current_.value(), this->absorption_current_a_.value());
          this->absorption_timer_.stop();
          return;
        }
        ESP_LOGV(TAG, "Starting absorption timer: for %i sec", this->absorption_timer_.time_s);
        
        this->absorption_timer_.start([this]() {
          ESP_LOGV(TAG, "Fired absorption timer: for %i sec. Swithing to FLOAT", this->absorption_timer_.time_s);
          this->call_update_state_later(BEFORE_FLOAT);
        });
      } else {
        ESP_LOGV(TAG, "Voltage don't reach absorption level: %.02f of %.02f", this->last_voltage_, this->absorption_voltage_v_.value_or(-1));
        this->absorption_timer_.stop();
      }

    break;
    case BEFORE_FLOAT:
      ESP_LOGD(TAG, "Charge status becoming FLOAT");
      if (this->voltage_target_sensor_ != nullptr) {
        this->voltage_target_sensor_->publish_state(this->float_voltage_.value_or(-1));
      }
      #ifdef USE_TEXT_SENSOR
      if (this->charge_state_sensor_ != nullptr) {
        this->charge_state_sensor_->publish_state("FLOAT");
      }
      #endif
      if (this->absorption_voltage_v_.has_value()) {
        ESP_LOGV(TAG, "Setting up absorption_restart_timer for %i", this->absorption_restart_timer_.time_s);
        this->absorption_restart_timer_.start([this]() {
          this->call_update_state_later(BEFORE_ABSORPTION);
        });
      }
      this->equaization_timeout_timer_.stop();
      this->equaization_timer_.stop();

      this->absorption_timer_.stop();
      this->absorption_low_voltage_timer_.stop();

      this->call_update_state_later(FLOAT);
    break;
    case FLOAT:
      ESP_LOGV(TAG, "Charge status FLOAT");
      if (this->absorption_voltage_v_.has_value() && this->absorption_restart_voltage_v_.has_value()) {
        if (this->last_voltage_ < absorption_restart_voltage_v_.value_or(-1)) {
          this->absorption_low_voltage_timer_.start([this]() {
            this->call_update_state_later(BEFORE_ABSORPTION);
          });
        } else {
          this->absorption_low_voltage_timer_.stop();
        }
      }

    break;
    case BEFORE_EQUAIZATION:
      ESP_LOGD(TAG, "Charge status becoming EQUAIZATION");
      this->absorption_restart_timer_.stop();
      this->absorption_timer_.stop();
      this->absorption_low_voltage_timer_.stop();

      if (this->voltage_target_sensor_ != nullptr) {
        this->voltage_target_sensor_->publish_state(this->equaization_voltage_v_.value_or(0));
      }
      #ifdef USE_TEXT_SENSOR
      if (this->charge_state_sensor_ != nullptr) {
        this->charge_state_sensor_->publish_state("EQUAIZATION");
      }
      #endif

      this->equaization_timeout_timer_.start([this]() {
        this->equaization_timer_.stop();
      
        this->equaization_interval_timer_.start([this]() {
          this->call_update_state_later(BEFORE_EQUAIZATION);
        });

        this->call_update_state_later(BEFORE_FLOAT);
      });
      this->call_update_state_later(EQUAIZATION);
    break;
    case EQUAIZATION:
      ESP_LOGV(TAG, "Charge status EQUAIZATION");
      if (this->last_voltage_ >= this->equaization_voltage_v_.value_or(0)) {
        ESP_LOGV(TAG, "Voltage level reaches equaization level: %.2f of %.2f", this->last_voltage_, this->equaization_voltage_v_.value_or(0));

        this->equaization_timer_.start([this]() {
          ESP_LOGV(TAG, "Equaization is complete. Switching to FLOAT (setup equaization_interval)");
          this->equaization_interval_timer_.start([this]() {
            this->call_update_state_later(BEFORE_EQUAIZATION);
          });
          this->equaization_timeout_timer_.stop();
          this->call_update_state_later(BEFORE_FLOAT);
        });
      } else {
        ESP_LOGV(TAG, "Voltage level is below equaization: %.2f of %.2f", this->last_voltage_, this->equaization_voltage_v_.value_or(0));

        this->equaization_timer_.stop();
      }
    break;
    case BEFORE_ERROR:
      ESP_LOGD(TAG, "Charge status becoming ERROR. Stopping all timers");
      this->absorption_restart_timer_.stop();
      this->absorption_timer_.stop();
      this->absorption_low_voltage_timer_.stop();

      this->equaization_timer_.stop();
      this->equaization_interval_timer_.stop();
      this->equaization_timeout_timer_.stop();

      if (this->voltage_target_sensor_ != nullptr) {
        this->voltage_target_sensor_->publish_state(0);
      }
      #ifdef USE_TEXT_SENSOR
      if (this->charge_state_sensor_ != nullptr) {
        this->charge_state_sensor_->publish_state("ERROR");
      }
      #endif

      this->status_set_error("Error state stoping...");

      this->call_update_state_later(ERROR);
    break;
    case ERROR:
      ESP_LOGV(TAG, "Charge status ERROR");
      if (this->voltage_auto_recovery_delay_timer_.time_s != 0 && (this->min_voltage_.has_value() || this->max_voltage_.has_value())) {
        auto is_min_ok = this->min_voltage_.has_value() ? this->last_voltage_ >= this->min_voltage_.value_or(-1) : true;
        auto is_max_ok = this->max_voltage_.has_value() ? this->last_voltage_ <= this->max_voltage_.value_or(-1) : true;

        if (is_min_ok && is_max_ok) {
          ESP_LOGV(TAG, "Voltage level reaches recovery conditions: %.2f of [%.2f, %.2f]", this->last_voltage_, this->min_voltage_.value_or(-1), this->max_voltage_.value_or(-1));
          this->voltage_auto_recovery_delay_timer_.start([this]() {
            this->status_clear_error();
            this->call_update_state_later(INITIAL);
          });
        } else {
          ESP_LOGV(TAG, "Voltage level DONT reaches recovery conditions: %.2f of [%.2f, %.2f]", this->last_voltage_, this->min_voltage_.value_or(-1), this->max_voltage_.value_or(-1));
          this->voltage_auto_recovery_delay_timer_.stop();
        }
      }

    break;
    default:
      
    ESP_LOGE(TAG, "Incorrect state");
    this->mark_failed();
    return;
  }
}

void ChargerComponent::call_update_state_later(CHARGE_STATES new_state) {
  this->set_timeout("UPDATE_STATUS", 16, [this, new_state]() {
    this->charge_state_ = new_state;
    this->updateState();
  });
}
  

} // 
} // esphome