#include <Arduino.h>
#include "LowPower.h"

class TON {
  public:
    uint32_t Preset = 0;

    TON(uint32_t Preset)
      : Preset(Preset){}
    TON(){}

    bool Update(bool IN, uint32_t curr_millis){
      if (!IN) {
        Reset();
        return false;
      }
      //saving starting time on r edge
      if (!r_edge) {
        starting_millis = curr_millis;
        r_edge = true;
      }
      // preventing elapsed to overlap
      if (!out) {
        elapsed = curr_millis - starting_millis;
        if (elapsed >= Preset) {
          out = true;
          elapsed = Preset;
        }
      }
      return out;
    }

    uint32_t Elapsed() {
      return elapsed;
    }

    void Reset() {
      out = false;
      elapsed = 0;
      starting_millis = 0;
      r_edge = false;
    }

    bool OUT() {
      return out;
    }
  private:
    bool out = false;
    uint32_t starting_millis = 0;
    bool r_edge = false;
    uint32_t elapsed = 0;
};

class TOF {
  public:
    uint32_t elapsed = 0;
    uint32_t Preset = 0;

    TOF(uint32_t Preset)
      : Preset(Preset){}
    TOF(){}

    bool Update(bool IN, uint32_t curr_millis){
      if (IN) {
        out = true;
        elapsed = 0;
        starting_millis = 0;
        f_edge = true;
        return out;
      }
      //detecting f edge of the input
      if (f_edge) {
        starting_millis = curr_millis;
        f_edge = false;
      }
      if (out) {
        elapsed = curr_millis - starting_millis;
        if (elapsed >= Preset) {
          // preventing elapsed to overlap
          elapsed = Preset;
          out = false;
        }
      }
      return out;
    }

    uint32_t Elapsed() {
      return elapsed;
    }

    void Reset() {
      out = false;
      elapsed = 0;
      starting_millis = 0;
      f_edge = false;
    }

    bool OUT() {
      return out;
    }
  private:
    bool out = false;
    uint32_t starting_millis = 0;
    bool f_edge = false;
};

// class processes digital input, has min on/off time to filter rapid changes of real signal
class DI {
  public:
    DI(uint8_t pin, uint32_t min_on_time, uint32_t min_off_time, bool invert)
      : pin(pin), min_on_time(min_on_time), min_off_time(min_off_time), invert(invert){}
    bool val();
    void read(const uint32_t curr_millis);
  private:
    bool out = false;
    uint8_t pin = 0;
    uint32_t min_on_time = 0;
    uint32_t min_off_time = 0;
    bool invert = false; // output inversion
    bool prev_val = false;
    TON min_timer;
    bool stored_val = false;
    bool start_timer = false;
    bool first_run = true;
};

bool DI::val() {
  return out;
}

void DI::read(const uint32_t curr_millis) {
  int curr_val = digitalRead(DI::pin);
  // init
  if (first_run) {
    out = curr_val != invert;
    prev_val = curr_val;
    first_run = false;
    return;
  }
  // reseting the timer on value change
  if (curr_val != prev_val) {
    min_timer.Reset();
    if (curr_val) {
      min_timer.Preset = min_on_time;
    } else {
      min_timer.Preset = min_off_time; 
    }
    start_timer = true;
    stored_val = curr_val;
  }
  prev_val = curr_val;

  if (min_timer.Update(start_timer, curr_millis)) {
    start_timer = false;
    out = stored_val != invert;
  }
}

class DO {
  public:
    DO(uint8_t pin, uint32_t min_on_time, uint32_t min_off_time)
      : pin(pin), min_on_time(min_on_time), min_off_time(min_off_time){}

    void write(bool val, uint32_t curr_millis) {
      if (!change_locked && (val != prev_val)) {
        prev_val = val;
        if(val) {
          change_timer.Preset = min_on_time;
        } else {
          change_timer.Preset = min_off_time;
        }
        change_locked = true;
        digitalWrite(pin, val);
      }
      if (change_timer.Update(change_locked, curr_millis)) {
        change_locked = false;
      }
    }
  private:
    uint32_t pin = 0;
    uint32_t min_on_time = 0;
    uint32_t min_off_time = 0;
    bool prev_val = false;
    bool change_locked = false;
    TON change_timer;
};

class Blinker {
  public:
    Blinker(uint32_t half_period)
      : blink_timer(half_period){}
    
    uint32_t get_half_period() {
      return blink_timer.Preset;
    }

    bool update(uint32_t curr_millis) {
      if (blink_timer.Preset == 0) {
        blink_timer.Reset();
        return false;
      }

      if (blink_timer.Update(true, curr_millis)) {
        out = !out;
        blink_timer.Reset();
      }
      return out;     
    }

    bool val() {
      return out;
    }

    void set_half_period(uint32_t new_half_period) {
      blink_timer.Preset = new_half_period;
    }
  private:
    TON blink_timer;
    bool out = false;
};

// Function determines actual time that watchdog timer count. This is especially important if wdt is used in cyclic power down,
// so adding that time is required every cycle to track time since start. Function controlled through serial. Any serial monitor
// that has timestamp can be used. Usage: put this function is setup, set constants and power down period, or alternatively make
// this function invoke on pin state. Follow instruction provided in the serial port and multiply power down period to determined
// factor and set corresponding value in your program. Example usage: 60ms * 1.17 = 70.2ms.
void time_calibration_mode() {
  constexpr uint32_t num_of_sleeps = 10; // set reasonable value that test take dozens of minutes
  constexpr uint32_t sleep_duration = 2000; // ms, set actual sleep period used
  constexpr uint32_t basic_delay_time = 2000; // 2s delay to let usb connect, don't forget to set the inteval in powerDown function!
  Serial.begin(115200);
  delay(basic_delay_time);
  Serial.println(F("Welcome to time calibration mode!"));
  Serial.println(F("To start the test set in serial monitor \"No Line Ending\" and send \"y\", or any key to skip to calculator:"));
  Serial.println(F("After the test has started USB will be disabled for the duration of the test."));
  while (!Serial.available()) {}
  if (Serial.readString() == "y") {
    Serial.println(F("Starting time test"));
    delay(basic_delay_time);
    UDCON |= (1 << DETACH);
    for (int i = 0; i < num_of_sleeps; i++) {
      LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_ON);
    }
    UDCON &= ~(1 << DETACH);
    delay(basic_delay_time);
    Serial.println(F("Test ended"));
  }
  while(true) {
    Serial.println(F("Calculator calculates relative time deviation factor, that has to be multiplied by basic sleep time to achieve accurate interval."));
    Serial.println(F("Legit value is between 0.8 and 1.2. Example usage: 60ms * 1.17 = 70.2ms"));
    Serial.println(F("If you make a mistake just send any symbol until you see welcome message again."));
    Serial.println(F("Send start time in format hh:mm:s.ms and hit enter:"));
    while (!Serial.available()) {}
    float start_time = Serial.readStringUntil(':').toFloat() * 3600000.0;
    start_time += Serial.readStringUntil(':').toFloat() * 60000.0;
    start_time += Serial.readString().toFloat() * 1000.0;
    Serial.println(F("Now send end time in format hh:mm:s.ms and hit enter:"));
    while (!Serial.available()) {}
    float end_time = Serial.readStringUntil(':').toFloat() * 3600000.0;
    end_time += Serial.readStringUntil(':').toFloat() * 60000.0;
    end_time += Serial.readString().toFloat() * 1000.0;
    float time_factor = ((float)end_time - start_time - basic_delay_time * 2.0) / (sleep_duration * num_of_sleeps);
    Serial.print(F("Factor is: "));
    Serial.println(time_factor);
    Serial.println(F("Send any symbol to start calculator again."));
    while (!Serial.available()) {};
    Serial.readString();
  }
}