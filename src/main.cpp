#include <Arduino.h>
#include "../lib/utils/src/utils.hpp"

constexpr uint32_t SECOND = 1000; // 1k ms in second
constexpr uint32_t MINUTE = 60000; // 60k milliseconds
constexpr uint8_t LOW_PRESSURE_PIN = 20;
constexpr uint8_t HIGH_PRESSURE_PIN = 21;
constexpr uint8_t MAINTENANCE_SWITCH_PIN = 9;
constexpr uint8_t PUMP_SOLENOID_CTRL_PIN = 19;
constexpr uint8_t LOW_PRESSURE_DIODE_PIN = 4;
constexpr uint8_t HIGH_PRESSURE_DIODE_PIN = 7;

class App {
  public:
    // progressive auto restart delays
    enum Waiting_time {
      NONE = 0,
      NORMAL = 10, //10
      PROLONGED = 30, //30
      LONG = 60
    };

    uint32_t current_millis = 0;
    DI low_pressure = DI(LOW_PRESSURE_PIN, SECOND / 2 , SECOND / 2, false);
    DI high_pressure = DI(HIGH_PRESSURE_PIN, SECOND / 2, SECOND / 2, false);
    DI maintenance_switch = DI(MAINTENANCE_SWITCH_PIN, SECOND / 2, SECOND / 2, true); // 500 ms
    DO pump_solenoid_ctrl = DO(PUMP_SOLENOID_CTRL_PIN, 2 * SECOND, 2 * SECOND);
    DO low_pressure_diode = DO(LOW_PRESSURE_DIODE_PIN, 0, 0);
    DO high_pressure_diode = DO(HIGH_PRESSURE_DIODE_PIN, 0, 0);
    enum Waiting_time restart_delay_time = NORMAL;
    uint8_t low_pressure_fault_count = 0;
    TON restart_delay_timer;
    TON operation_timer = TON(60 * MINUTE); // max operation time
    uint8_t state = 0;
    bool pump_solenoid_out = LOW;
    bool yellow_diode = false;
    bool red_diode = false;
    Blinker blinker = Blinker(0.25 * SECOND);
    TON high_pressure_drop = TON(30 * SECOND);

    void setup() {
      pinMode(LOW_PRESSURE_PIN, INPUT_PULLUP);
      pinMode(HIGH_PRESSURE_PIN, INPUT_PULLUP);
      pinMode(MAINTENANCE_SWITCH_PIN, INPUT_PULLUP);
      pinMode(LOW_PRESSURE_DIODE_PIN, OUTPUT);
      pinMode(HIGH_PRESSURE_DIODE_PIN, OUTPUT);
      // setting main output pin to low to prevent high state
      digitalWrite(PUMP_SOLENOID_CTRL_PIN, LOW);
      pinMode(PUMP_SOLENOID_CTRL_PIN, OUTPUT);
      // turning off red rx/tx diodes that draw 500mW on ProMicro!
      pinMode(LED_BUILTIN_RX, INPUT);
      pinMode(LED_BUILTIN_TX, INPUT);
    }

    void loop() {
      current_millis = millis();

      //reading inputs
      low_pressure.read(current_millis);
      high_pressure.read(current_millis);
      maintenance_switch.read(current_millis);

      // updating blinker
      blinker.update(current_millis);

      // yellow diode display high pressure sensor by default
      yellow_diode = high_pressure.val();
      // red diode display low pressure sensor by default
      red_diode = low_pressure.val();
      // Monitor high pressure drop to prevent frequent short switching on/off
      // while actual pressure still enough to trip the switch.
      // Pressure has to stay low for "preset" time at least to start actuators
      // in normal mode.
      high_pressure_drop.Update(!high_pressure.val(), current_millis);

      switch (state) {
        case 0:
        // idle
          restart_delay_timer.Reset();
          operation_timer.Reset();
          pump_solenoid_out = LOW;
          if ((maintenance_switch.val() && !high_pressure.val())
              || (!low_pressure.val() && high_pressure_drop.OUT())) {
            state = 10;
            break;
          }
          break;
        case 10:
        // operation
          pump_solenoid_out = HIGH;
          if (low_pressure.val() && !maintenance_switch.val()) {
            if (low_pressure_fault_count <= 2) {
              low_pressure_fault_count++;
            }
            state = 20;
            break;        
          }
          if (high_pressure.val()) {
            state = 20;
            low_pressure_fault_count = 0;
            break;
          }
          if (operation_timer.Update(true, current_millis)) {
            state = 100;
          }
          break;
        case 20:
        // restart delay
          pump_solenoid_out = LOW;
          if (maintenance_switch.val()) {
            state = 0;
            low_pressure_fault_count = 0;
            break;
          }
          switch (low_pressure_fault_count) {
            case 0:
            case 1:
              restart_delay_time = NORMAL;
              break;
            case 2:
              restart_delay_time = PROLONGED;
              break;
            default: 
              restart_delay_time = LONG;
          }
          restart_delay_timer.Preset = restart_delay_time * MINUTE;
          if (restart_delay_timer.Update(true, current_millis)) {
            state = 0;
            break;
          }
          if (low_pressure_fault_count > 1) {
            red_diode = blinker.val();
          }
          break;
        case 100:
        // fault: unable to build pressure in time
          pump_solenoid_out = LOW;
          yellow_diode = blinker.val();
          if (maintenance_switch.val()) {
            state = 0;
            break;
          }
          break;
        default: 
          state = 0;
      };
      // writing outputs
      pump_solenoid_ctrl.write(pump_solenoid_out, current_millis);
      low_pressure_diode.write(red_diode, current_millis);
      high_pressure_diode.write(yellow_diode, current_millis);
    }
};

App app;

void setup() {
  app.setup();
}

void loop() {
  app.loop();
}
