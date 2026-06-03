#include <P1AM.h>
#include <RTCZero.h>

constexpr uint32_t kDebounceMillis = 100;
constexpr uint32_t kOnTimeSec = 60 * 10;
constexpr uint8_t kInputSlot = 1;
constexpr uint8_t kOutputSlot = 2;
constexpr int kNumSwitches = 3;
constexpr int kNumRelays = 10;

RTCZero rtc;

enum class SwitchState{
  kOpen,
  kClosed,
  kCloseEvent,  // Triggered the first polling loop the switch is closed.
  kOpenEvent,  // Triggered the first polling the switch is opened.
};

class Switch {
  public:
    Switch(uint8_t slot, uint8_t channel, uint32_t debounce_millis):
      slot_(slot), channel_(channel), debounce_millis_(debounce_millis) {}

    SwitchState Poll() {
      if (P1.readDiscrete(slot_, channel_) > 0){
        is_opening_ = false;
        if (state_ != SwitchState::kClosed) {
          if (!is_closing_) {
            is_closing_ = true;
            first_closed_millis_ = millis();
          } else if (millis() - first_closed_millis_ > debounce_millis_) {
            state_ = SwitchState::kClosed;
            return SwitchState::kCloseEvent;
          }
        }
      } else {
        is_closing_ = false;
        if (state_ != SwitchState::kOpen) {
          if (!is_opening_) {
            is_opening_ = true;
            first_closed_millis_ = millis();
          } else if (millis() - first_opened_millis_ > debounce_millis_) {
            state_ = SwitchState::kOpen;
            return SwitchState::kOpenEvent;
          }
        }
      }
      return state_;
    }

  private:
    uint8_t slot_, channel_;
    int32_t debounce_millis_;
    SwitchState state_ = SwitchState::kOpen;
    bool is_closing_ = false, is_opening_ = false;
    uint32_t first_closed_millis_ = 0, first_opened_millis_ = 0;
};

class LightController {
  public:
    LightController(): switches_{
      Switch(kInputSlot, 1, kDebounceMillis),
      Switch(kInputSlot, 2, kDebounceMillis),
      Switch(kInputSlot, 3, kDebounceMillis)
    } {}

    void Poll(uint32_t unix_time_s) {
      for (int ind = 1; ind <= kNumSwitches; ind++) {
        SwitchState state = switches_[ind].Poll();
        if (state == SwitchState::kCloseEvent) {
          Serial.println("Switch press detected");
          ToggleLights(unix_time_s);
        }
      }
      MaybePowerOffLights(unix_time_s);
    }

  private:
    // Powers off lights if we have exceeded the max on-time.
    void MaybePowerOffLights (uint32_t unix_time_s) {
      if (outputs_on_ && unix_time_s > deadline_s_) {
        PowerOffAll();
      }
    }

    // Toggles the light state.
    void ToggleLights(uint32_t unix_time_s) {
      if (outputs_on_) {
        PowerOffAll();
      } else {
        PowerOnAll();
        deadline_s_ = unix_time_s;
      }
    }

    void PowerOnAll() {
      if (!outputs_on_) {
        Serial.println("Powering on all channels.");
        outputs_on_ = true;
        for (int ch = 1; ch <= kNumRelays; ch++) {
          P1.writeDiscrete(HIGH, kOutputSlot, ch);
        }
      }
    };

    void PowerOffAll() {
      if (outputs_on_) {
        Serial.println("Powering off all channels.");
        outputs_on_ = false;
        for (int ch = 1; ch <= kNumRelays; ch++) {
          P1.writeDiscrete(LOW, kOutputSlot, ch);
        }
      }
    };

    Switch switches_[kNumSwitches];
    uint32_t deadline_s_ = 0;
    bool outputs_on_ = false;
} controller;

void setup() {
  Serial.begin(115200);  //initialize serial communication at 115200 bits per second 
  while (!P1.init()){ 
    ; //Wait for Modules to Sign on   
  }
  rtc.begin();
}

void loop() {
  controller.Poll(rtc.getEpoch());
  delay(10);
}
