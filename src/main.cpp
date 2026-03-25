#include <Arduino.h>
#include "../lib/utils/src/utils.hpp"
#include "LowPower.h"
#include "avr/wdt.h"

constexpr uint32_t SECOND = 1000; // 1k ms in second
constexpr uint32_t MINUTE = 60000; // 60k milliseconds
constexpr uint8_t watchdog_time = WDTO_2S;
// interval calibrated on real mc cuz default is inaccurate
constexpr uint32_t execution_interval = 71; // ms
constexpr uint8_t LOW_PRESSURE_PIN = 20;
constexpr uint8_t HIGH_PRESSURE_PIN = 21;
constexpr uint8_t MAINTENANCE_SWITCH_PIN = 9;
constexpr uint8_t PUMP_SOLENOID_CTRL_PIN = 19;
constexpr uint8_t LOW_PRESSURE_DIODE_PIN = 4;
constexpr uint8_t HIGH_PRESSURE_DIODE_PIN = 7;

class App {
  public:
    // progressive auto restart delays in minutes
    enum Waiting_time {
      NONE = 0,
      NORMAL = 5, 
      PROLONGED = 30, //30
      LONG = 60
    };

    uint32_t system_time = 0;
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
    // noticeable pressure drop
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
      // sleeping to be able to connect to USB w/o reset
      delay(5 * SECOND);
      // enabling watchdog timer
      wdt_enable(watchdog_time);
      // disabling usb to prevent usb reconnects due to powerdowns
      UDCON |= (1 << DETACH);
    }

    void loop() {
      // kick the dog
      wdt_reset();

      // saving starting time
      uint32_t start_time = millis();

      //reading inputs
      low_pressure.read(system_time);
      high_pressure.read(system_time);
      maintenance_switch.read(system_time);

      // updating blinker
      blinker.update(system_time);

      // yellow diode display high pressure sensor by default
      yellow_diode = high_pressure.val();
      // red diode display low pressure sensor by default
      red_diode = low_pressure.val();
      // Monitor high pressure noticeable drop to prevent frequent short switching on/off
      // due to actual pressure still enough to trip the high pressure switch.
      // Pressure has to stay low for "preset" time at least to start actuators
      // in normal mode.
      high_pressure_drop.Update(!high_pressure.val(), system_time);

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
          if (operation_timer.Update(true, system_time)) {
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
          if (restart_delay_timer.Update(true, system_time)) {
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
      pump_solenoid_ctrl.write(pump_solenoid_out, system_time);
      low_pressure_diode.write(red_diode, system_time);
      high_pressure_diode.write(yellow_diode, system_time);

      // disabling watchdog before powerdown to prevent unwanted resets
      wdt_disable();
      // powering down mc to save power
      LowPower.powerDown(SLEEP_60MS, ADC_OFF, BOD_ON);
      // reeanabling watchdog again
      wdt_enable(watchdog_time);
      // updating time
      system_time += execution_interval + (millis() - start_time);
    }
};

App app;

void setup() {
  app.setup();
}

void loop() {
  app.loop();
}