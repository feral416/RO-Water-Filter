#include <Arduino.h>

class TON {
  public:
    uint32_t Elapsed = 0;
    uint32_t Preset = 0;

    TON(uint32_t Preset)
      : Preset(Preset){}
    TON(){}

    bool Update(bool IN, uint32_t curr_millis){
      if (!IN) {
        Reset();
        return false;
      }
      //detecting r edge of the input
      if (IN && !prev_in) {
        starting_millis = curr_millis;
      }
      prev_in = IN;
      // preventing elapsed to overlap
      if (!out) {
        Elapsed = curr_millis - starting_millis;
        if (Elapsed >= Preset) {
          out = true;
          Elapsed = Preset;
        }
      }
      return out;
    }

    void Reset() {
      out = false;
      Elapsed = 0;
      starting_millis = 0;
      prev_in = false;
    }

    bool OUT() {
      return out;
    }
  private:
    bool out = false;
    uint32_t starting_millis = 0;
    bool prev_in = false;
};

class TOF {
  public:
    uint32_t Elapsed = 0;
    uint32_t Preset = 0;

    TOF(uint32_t Preset)
      : Preset(Preset){}
    TOF(){}

    bool Update(bool IN, uint32_t curr_millis){
      if (IN) {
        out = true;
        Elapsed = 0;
        starting_millis = 0;
        prev_in = true;
        return out;
      }
      //detecting f edge of the input
      if (!IN && prev_in) {
        starting_millis = curr_millis;
        out = true;
      }
      prev_in = false;
      // preventing elapsed to overlap
      if (out) {
        Elapsed = curr_millis - starting_millis;
        if (Elapsed >= Preset) {
          Elapsed = Preset;
          out = false;
        }
      }
      return out;
    }

    void Reset() {
      out = false;
      Elapsed = 0;
      starting_millis = 0;
      prev_in = false;
    }

    bool OUT() {
      return out;
    }
  private:
    bool out = false;
    uint32_t starting_millis = 0;
    bool prev_in = false;
};

// class processess digital input, has min on/off time to filter rapid changes of real signal
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