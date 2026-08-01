// Tests for debouncing and short/long press classification.
#include "../SmartSocket/src/core/ButtonDebouncer.h"

#include "../SmartSocket/src/core/Config.h"
#include "Fakes.h"
#include "TestFramework.h"

using namespace smartsocket;
using namespace smartsocket::fakes;

namespace {

// Holds a level for a span, returning the first non-None event seen.
ButtonEvent hold(ButtonDebouncer& b, Millis& now, bool pressed, Millis durationMs,
                 Millis stepMs = 5) {
  const Millis end = now + durationMs;
  ButtonEvent found = ButtonEvent_None;
  while (now < end) {
    now += stepMs;
    const ButtonEvent e = b.update(now, pressed);
    if (e != ButtonEvent_None && found == ButtonEvent_None) {
      found = e;
    }
  }
  return found;
}

}  // namespace

TEST(a_clean_short_press_fires_on_release) {
  ButtonDebouncer b;
  Millis now = 0;

  CHECK_EQ(hold(b, now, false, 100), ButtonEvent_None);
  CHECK_EQ(hold(b, now, true, 200), ButtonEvent_None);  // nothing yet, still held
  CHECK_EQ(hold(b, now, false, 100), ButtonEvent_ShortPress);
}

TEST(contact_bounce_does_not_produce_extra_presses) {
  ButtonDebouncer b;
  Millis now = 0;
  hold(b, now, false, 100);

  // Real buttons chatter for a few milliseconds on contact. Each bounce is
  // shorter than the debounce window, so none of it should register.
  for (int i = 0; i < 6; ++i) {
    now += 2;
    CHECK_EQ(b.update(now, true), ButtonEvent_None);
    now += 2;
    CHECK_EQ(b.update(now, false), ButtonEvent_None);
  }

  hold(b, now, true, 200);
  CHECK_EQ(hold(b, now, false, 100), ButtonEvent_ShortPress);
}

TEST(a_long_press_fires_while_still_held) {
  ButtonDebouncer b;
  Millis now = 0;
  hold(b, now, false, 100);

  // Firing mid-hold gives the user feedback without making them let go first.
  CHECK_EQ(hold(b, now, true, config::ButtonLongPressMs + 100),
           ButtonEvent_LongPress);
}

TEST(a_long_press_does_not_also_fire_a_short_press_on_release) {
  ButtonDebouncer b;
  Millis now = 0;
  hold(b, now, false, 100);
  CHECK_EQ(hold(b, now, true, config::ButtonLongPressMs + 100),
           ButtonEvent_LongPress);

  // One hold must yield exactly one event. A trailing short press would toggle
  // manual-off straight back off again.
  CHECK_EQ(hold(b, now, false, 200), ButtonEvent_None);
}

TEST(a_long_press_fires_only_once_however_long_it_is_held) {
  ButtonDebouncer b;
  Millis now = 0;
  hold(b, now, false, 100);
  CHECK_EQ(hold(b, now, true, config::ButtonLongPressMs + 100),
           ButtonEvent_LongPress);

  CHECK_EQ(hold(b, now, true, 5000), ButtonEvent_None);
}

TEST(a_press_just_under_the_long_threshold_stays_short) {
  ButtonDebouncer b;
  Millis now = 0;
  hold(b, now, false, 100);
  CHECK_EQ(hold(b, now, true, config::ButtonLongPressMs - 200), ButtonEvent_None);
  CHECK_EQ(hold(b, now, false, 100), ButtonEvent_ShortPress);
}

TEST(consecutive_presses_are_reported_separately) {
  ButtonDebouncer b;
  Millis now = 0;
  hold(b, now, false, 100);

  for (int i = 0; i < 3; ++i) {
    hold(b, now, true, 200);
    CHECK_EQ(hold(b, now, false, 200), ButtonEvent_ShortPress);
  }
}

TEST(the_bank_debounces_each_button_independently) {
  FakeButtonSource source;
  ButtonBank bank(source);
  Millis now = 0;

  for (int i = 0; i < 30; ++i) {
    now += 5;
    bank.update(now);
  }

  source.press(Button_Next);
  for (int i = 0; i < 40; ++i) {
    now += 5;
    bank.update(now);
  }
  source.release(Button_Next);

  bool sawNext = false;
  for (int i = 0; i < 40; ++i) {
    now += 5;
    bank.update(now);
    if (bank.event(Button_Next) == ButtonEvent_ShortPress) {
      sawNext = true;
    }
    // The untouched button must stay silent.
    CHECK_EQ(bank.event(Button_Action), ButtonEvent_None);
  }
  CHECK(sawNext);
}
