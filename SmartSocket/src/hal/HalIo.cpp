#if defined(ARDUINO)

#include "HalIo.h"

namespace smartsocket {

HalRelay::HalRelay(uint8_t pin, bool activeLow)
    : pin_(pin), activeLow_(activeLow), closed_(false) {}

void HalRelay::begin() {
  // Order matters: drive the de-energized level first, then enable the output.
  // Doing it the other way round leaves the pin briefly at its default LOW, which
  // on an active-LOW board would snap the contacts closed during boot.
  digitalWrite(pin_, activeLow_ ? HIGH : LOW);
  pinMode(pin_, OUTPUT);
  digitalWrite(pin_, activeLow_ ? HIGH : LOW);
  closed_ = false;
}

void HalRelay::setClosed(bool closed) {
  closed_ = closed;
  const bool level = activeLow_ ? !closed : closed;
  digitalWrite(pin_, level ? HIGH : LOW);
}

HalBuzzer::HalBuzzer(uint8_t pin) : pin_(pin) {}

void HalBuzzer::begin() {
  digitalWrite(pin_, LOW);
  pinMode(pin_, OUTPUT);
  digitalWrite(pin_, LOW);
}

void HalBuzzer::setSounding(bool sounding) {
  // The buzzer in this kit is PASSIVE: it has no oscillator of its own, so a
  // steady HIGH produces one click and then silence. tone() drives it with a
  // square wave instead. 1500 Hz is where this element measured loudest.
  if (sounding) {
    tone(pin_, BuzzerToneHz);
  } else {
    noTone(pin_);
    digitalWrite(pin_, LOW);
  }
}

HalButtons::HalButtons(uint8_t nextPin, uint8_t actionPin) {
  pins_[Button_Next] = nextPin;
  pins_[Button_Action] = actionPin;
}

void HalButtons::begin() {
  for (uint8_t i = 0; i < Button_Count; ++i) {
    pinMode(pins_[i], INPUT_PULLUP);
  }
}

bool HalButtons::isPressed(ButtonId id) const {
  if (id >= Button_Count) {
    return false;
  }
  // Active-low: the internal pull-up holds the pin HIGH until the button ties it
  // to GND.
  return digitalRead(pins_[id]) == LOW;
}

}  // namespace smartsocket

#endif  // ARDUINO
