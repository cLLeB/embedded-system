#include "ButtonDebouncer.h"

#include "Config.h"

namespace smartsocket {

ButtonDebouncer::ButtonDebouncer()
    : lastChangeMs_(0),
      pressedSinceMs_(0),
      stableLevel_(false),
      candidateLevel_(false),
      longFired_(false) {}

ButtonEvent ButtonDebouncer::update(Millis nowMs, bool rawPressed) {
  // Any change restarts the settling window; the level only counts once it has
  // held still for the full debounce period.
  if (rawPressed != candidateLevel_) {
    candidateLevel_ = rawPressed;
    lastChangeMs_ = nowMs;
    return ButtonEvent_None;
  }

  const bool settled = (nowMs - lastChangeMs_) >= config::ButtonDebounceMs;
  if (!settled || candidateLevel_ == stableLevel_) {
    // Level is unchanged and settled. A long press fires mid-hold so the user gets
    // feedback without releasing.
    if (stableLevel_ && !longFired_ &&
        (nowMs - pressedSinceMs_) >= config::ButtonLongPressMs) {
      longFired_ = true;
      return ButtonEvent_LongPress;
    }
    return ButtonEvent_None;
  }

  // Confirmed transition.
  stableLevel_ = candidateLevel_;

  if (stableLevel_) {
    pressedSinceMs_ = nowMs;
    longFired_ = false;
    return ButtonEvent_None;  // nothing is decided until release or hold
  }

  // Released. If the long press already fired, swallow the release so a single
  // hold cannot also register as a short press.
  if (longFired_) {
    longFired_ = false;
    return ButtonEvent_None;
  }
  return ButtonEvent_ShortPress;
}

ButtonBank::ButtonBank(IButtonSource& source) : source_(source) {
  for (uint8_t i = 0; i < Button_Count; ++i) {
    events_[i] = ButtonEvent_None;
  }
}

void ButtonBank::update(Millis nowMs) {
  for (uint8_t i = 0; i < Button_Count; ++i) {
    const ButtonId id = static_cast<ButtonId>(i);
    events_[i] = debouncers_[i].update(nowMs, source_.isPressed(id));
  }
}

ButtonEvent ButtonBank::event(ButtonId id) const {
  if (id >= Button_Count) {
    return ButtonEvent_None;
  }
  return events_[id];
}

}  // namespace smartsocket
