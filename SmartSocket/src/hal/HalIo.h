// HalIo.h - clock, relay, buzzer and buttons on real pins.
#ifndef SMARTSOCKET_HAL_HALIO_H
#define SMARTSOCKET_HAL_HALIO_H

#if defined(ARDUINO)

#include <Arduino.h>

#include "../core/Interfaces.h"
#include "../core/Types.h"

namespace smartsocket {

class HalClock : public IClock {
 public:
  Millis now() const { return millis(); }
};

class HalRelay : public IRelay {
 public:
  HalRelay(uint8_t pin, bool activeLow);

  // Sets the pin to the de-energized level BEFORE switching to OUTPUT, so the
  // relay cannot click closed for a few microseconds during boot.
  void begin();

  void setClosed(bool closed);
  bool isClosed() const { return closed_; }

 private:
  uint8_t pin_;
  bool activeLow_;
  bool closed_;
};

// Drive frequency for the passive buzzer. A passive element is resonant: a few
// hundred Hz off its peak and it is barely audible. 1500 Hz is where the one in
// this build measured loudest. If a different buzzer is ever fitted, run
// BuzzerTest/BuzzerTest.ino, note which tone is loudest, and change this.
const uint16_t BuzzerToneHz = 1500;

class HalBuzzer : public IBuzzer {
 public:
  explicit HalBuzzer(uint8_t pin);
  void begin();
  void setSounding(bool sounding);

 private:
  uint8_t pin_;
};

// Buttons wired to GND with INPUT_PULLUP: a pressed button reads LOW. That
// inversion is hidden here so the core never thinks about pull-ups.
class HalButtons : public IButtonSource {
 public:
  HalButtons(uint8_t nextPin, uint8_t actionPin);
  void begin();
  bool isPressed(ButtonId id) const;

 private:
  uint8_t pins_[Button_Count];
};

}  // namespace smartsocket

#endif  // ARDUINO
#endif  // SMARTSOCKET_HAL_HALIO_H
