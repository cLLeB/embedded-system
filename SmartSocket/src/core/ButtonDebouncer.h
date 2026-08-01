// ButtonDebouncer.h - turns noisy raw button levels into clean press events.
//
// Lives in the core rather than the HAL so debounce and long-press timing can be
// tested without pressing anything.
//
// Portable C++: must not include Arduino.h.
#ifndef SMARTSOCKET_CORE_BUTTONDEBOUNCER_H
#define SMARTSOCKET_CORE_BUTTONDEBOUNCER_H

#include "Interfaces.h"
#include "Types.h"

namespace smartsocket {

// Tracks one button. A long press fires as soon as the hold threshold is reached,
// rather than waiting for release, so the user gets feedback while still holding.
// The subsequent release is then swallowed so one hold never yields two events.
class ButtonDebouncer {
 public:
  ButtonDebouncer();

  ButtonEvent update(Millis nowMs, bool rawPressed);

 private:
  Millis lastChangeMs_;
  Millis pressedSinceMs_;
  bool stableLevel_;
  bool candidateLevel_;
  bool longFired_;
};

// Convenience wrapper: debounces every button in one call against an IButtonSource.
class ButtonBank {
 public:
  explicit ButtonBank(IButtonSource& source);

  void update(Millis nowMs);
  ButtonEvent event(ButtonId id) const;

 private:
  IButtonSource& source_;
  ButtonDebouncer debouncers_[Button_Count];
  ButtonEvent events_[Button_Count];
};

}  // namespace smartsocket

#endif  // SMARTSOCKET_CORE_BUTTONDEBOUNCER_H
