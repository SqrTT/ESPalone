# Battery Charger Component for ESPHome

This component provides a configurable multi-stage battery charging controller for ESPHome. It implements absorption, float, and equalization charging strategies to properly maintain various battery types including lead-acid, AGM, and LiFePO4.

## Features
- Multi-stage charging logic (absorption, float, equalization)
- Voltage and current-based state transitions
- Configurable voltage thresholds and timers
- Auto-recovery from error states
- Status monitoring via sensor outputs
- Safety limits with min/max voltage protection

## Installation

Add this component to your ESPHome project using the `external_components` configuration:

```yaml
external_components:
  - source: 
      type: git
      url: https://github.com/SqrTT/ESPalone/components
      ref: main
    components: ['battery_charger']
```


## Configuration Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `voltage_sensor` | ID | Required | Sensor that provides battery voltage readings |
| `current_sensor` | ID | Optional | Sensor that provides charging current readings |
| `float_voltage` | Voltage | Required | Target voltage during float stage |
| `target_voltage_sensor` | sensor | Optional | Sensor that will display target voltage |
| `charge_state_sensor` | ID | Optional | Text sensor to display the current charging state |
| `voltage_max` | Voltage | Optional | Maximum safe voltage, exceeding triggers error state |
| `voltage_min` | Voltage | Optional | Minimum safe voltage, falling below triggers error state |
| `voltage_auto_recovery_delay` | Time | `0s` | Delay before auto-recovery from error state once voltage is in safe range |
| `absorption_voltage` | Voltage | Optional | Target voltage during absorption stage |
| `absorption_current` | Current | Optional | Current threshold to maintain absorption stage |
| `absorption_time` | Time | Optional | Duration to maintain absorption stage before switching to float |
| `absorption_restart_voltage` | Voltage | Optional | Voltage threshold below which to restart absorption stage |
| `absorption_restart_time` | Time | Optional | Interval after which to automatically restart absorption from float |
| `absorption_low_voltage_delay` | Time | `1min` | Delay before switching to absorption after voltage drops below restart threshold |
| `equalization_voltage` | Voltage | Optional | Target voltage during equalization stage |
| `equalization_time` | Time | `1h` | Duration of the equalization stage |
| `equalization_interval` | Time | `7d` | Interval between equalization cycles |
| `equalization_timeout` | Time | `3h` | Maximum time to attempt reaching equalization voltage |

## Example: Lead Acid Battery (12V)

```yaml
# Lead Acid Battery configuration example
battery_charger:
  voltage_sensor: battery_voltage_sensor
  current_sensor: battery_current_sensor
  float_voltage: 13.6V
  absorption_voltage: 14.4V
  absorption_current: 2A
  absorption_restart_voltage: 12.4V
  absorption_time: 60min
  absorption_restart_time: 7d
  voltage_max: 15.0V
  voltage_min: 10.5V
  voltage_auto_recovery_delay: 5min
  equalization_voltage: 14.8V
  equalization_time: 2h
  equalization_interval: 30d
  equalization_timeout: 5h
  target_voltage_sensor:
    id: target_voltage
    name: "Target Charging Voltage"
  charge_state_sensor: charge_state

text_sensor:
  - platform: template
    id: charge_state
    name: "Charging State"
```

## Example: LiFePO4 Battery (12.8V - 4S)

```yaml
# LiFePO4 Battery configuration example
battery_charger:
  voltage_sensor: lifepo4_voltage_sensor
  current_sensor: lifepo4_current_sensor
  float_voltage: 13.6V
  absorption_voltage: 14.2V
  absorption_current: 5A  # 0.05C Example: 0.05 x 100Ah = 5A
  absorption_restart_voltage: 13.2V
  absorption_time: 30min
  voltage_max: 14.6V
  voltage_min: 10.0V
  voltage_auto_recovery_delay: 10min
  # Note: Equalization is typically not used for LiFePO4 batteries
  target_voltage_sensor:
    id: target_voltage_lifepo4
    name: "LiFePO4 Target Voltage"
  charge_state_sensor: lifepo4_charge_state

text_sensor:
  - platform: template
    id: lifepo4_charge_state
    name: "LiFePO4 Charging State"
```

## Example: LiFePO4 (3.2V Single Cell)

```yaml
# LiFePO4 Single Cell configuration
battery_charger:
  voltage_sensor: lifepo4_cell_voltage
  current_sensor: lifepo4_cell_current
  float_voltage: 3.35V
  absorption_voltage: 3.55V
  absorption_current: 0.02C  # Example: 0.02 x 100Ah = 2A
  absorption_restart_voltage: 3.3V
  absorption_time: 30min
  voltage_max: 3.65V
  voltage_min: 2.5V
  voltage_auto_recovery_delay: 5min
  target_voltage_sensor:
    id: cell_target_voltage
    name: "Cell Target Voltage"
  charge_state_sensor: cell_charge_state

text_sensor:
  - platform: template
    id: cell_charge_state
    name: "Cell Charging State"
```

## Charging States

The component transitions through the following states:

1. **ABSORPTION**: Charges battery at constant voltage (absorption_voltage) until current drops below threshold or timer completes
2. **FLOAT**: Maintains battery at lower voltage (float_voltage) for long-term maintenance
3. **EQUALIZATION**: Periodically applies higher voltage to balance cells and remove sulfation (primarily for lead-acid batteries)
4. **ERROR**: Safety state when voltage exceeds limits, with optional auto-recovery

## Integration Example

This component works well with other ESPHome components like:

```yaml
# Example integration with voltage divider sensor and output control
sensor:
  - platform: adc
    pin: A0
    id: battery_voltage_adc
    update_interval: 5s
    filters:
      - multiply: 5.67  # Example voltage divider calibration
    name: "Battery Voltage"
  
  - platform: adc
    pin: A1
    id: battery_current_adc
    update_interval: 5s
    filters:
      - calibrate_linear:
          - 0.0 -> 0.0
          - 2.5 -> 10.0  # Example for ACS712 or similar
    name: "Battery Current"


# Connect battery_charger with voltage and current sensors
battery_charger:
  voltage_sensor: battery_voltage_adc
  current_sensor: battery_current_adc
  float_voltage: 13.6V
  # ... other settings
```

## Safety Notes

* Always use appropriate hardware protections (fuses, overvoltage protection, etc.) in addition to this software control
* Test thoroughly with your specific battery chemistry before deployment
* Monitor temperature for lead-acid batteries, especially during equalization
* LiFePO4 batteries typically should not use equalization charging

## Contributing

Contributions and feedback are welcome! Please feel free to submit a pull request or open an issue.

## License

This component is released under the MIT License.