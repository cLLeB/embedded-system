// BuzzerDriver.h - non-blocking beep patterns for an active buzzer.
//
// An active buzzer has its own oscillator, so it needs only on/off - no tone().
// Every pattern is driven from a timestamp rather than delay(), because blocking
// here would stall current sampling and the relay decision.
//
// Portable C++: must not include Arduino.h.
#ifndef SMARTSOCKET_CORE_BUZZERDRIVER_H
#define SMARTSOCKET_CORE_BUZZERDRIVER_H

#include "Interfaces.h"
#include "Types.h"

namespace smartsocket {

class BuzzerDriver {
 public:
  explicit BuzzerDriver(IBuzzer& buzzer);

  // Starts a pattern. Re-requesting the pattern already playing is ignored, so
  // calling this every loop iteration will not restart it.
  void play(BuzzerPattern pattern, Millis nowMs);

  void silence(Millis nowMs);

  // Advances the pattern. Must be called every loop iteration.
  void update(Millis nowMs);

  BuzzerPattern current() const { return pattern_; }
  bool isSounding() const { return sounding_; }

 private:
  void apply(bool sounding);

  IBuzzer& buzzer_;
  BuzzerPattern pattern_;
  Millis patternStartMs_;
  bool sounding_;
};

}  // namespace smartsocket

#endif  // SMARTSOCKET_CORE_BUZZERDRIVER_H
