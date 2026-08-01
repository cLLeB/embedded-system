// Tests for non-blocking buzzer patterns.
#include "../SmartSocket/src/core/BuzzerDriver.h"

#include "Fakes.h"
#include "TestFramework.h"

using namespace smartsocket;
using namespace smartsocket::fakes;

namespace {

// Steps the driver to `target`, reporting whether it ever sounded on the way.
bool runUntil(BuzzerDriver& d, Millis& now, Millis target, Millis stepMs = 10) {
  bool sounded = false;
  while (now < target) {
    now += stepMs;
    d.update(now);
    if (d.isSounding()) {
      sounded = true;
    }
  }
  return sounded;
}

}  // namespace

TEST(the_buzzer_starts_silent) {
  FakeBuzzer buzzer;
  BuzzerDriver driver(buzzer);
  CHECK_FALSE(driver.isSounding());
  CHECK_EQ(driver.current(), Buzz_Silent);
}

TEST(plug_detect_chirps_once_then_stops) {
  FakeBuzzer buzzer;
  BuzzerDriver driver(buzzer);
  Millis now = 1000;

  driver.play(Buzz_PlugDetected, now);
  CHECK(driver.isSounding());

  CHECK(runUntil(driver, now, 1050));
  runUntil(driver, now, 1200);
  CHECK_FALSE(driver.isSounding());
}

TEST(the_cutoff_pattern_beeps_then_falls_silent) {
  FakeBuzzer buzzer;
  BuzzerDriver driver(buzzer);
  Millis now = 0;

  driver.play(Buzz_Cutoff, now);
  CHECK(runUntil(driver, now, 1000));

  // The pattern is finite: a socket that beeps forever after a normal, correct
  // cutoff would be unusable overnight.
  runUntil(driver, now, 5000);
  CHECK_FALSE(driver.isSounding());
}

TEST(the_fault_pattern_never_stops_on_its_own) {
  FakeBuzzer buzzer;
  BuzzerDriver driver(buzzer);
  Millis now = 0;

  driver.play(Buzz_Fault, now);

  // A fault is a wiring problem. It must keep demanding attention until a human
  // clears it - which is exactly why silence has to be an explicit command.
  CHECK(runUntil(driver, now, 30000));
  bool soundedLate = false;
  for (int i = 0; i < 200; ++i) {
    now += 10;
    driver.update(now);
    if (driver.isSounding()) {
      soundedLate = true;
    }
  }
  CHECK(soundedLate);
}

TEST(silence_stops_a_fault_immediately) {
  FakeBuzzer buzzer;
  BuzzerDriver driver(buzzer);
  Millis now = 0;

  driver.play(Buzz_Fault, now);
  runUntil(driver, now, 200);

  driver.silence(now);
  CHECK_FALSE(driver.isSounding());
  CHECK_FALSE(buzzer.isSounding());

  runUntil(driver, now, 10000);
  CHECK_FALSE(driver.isSounding());
}

TEST(replaying_the_active_pattern_does_not_restart_it) {
  FakeBuzzer buzzer;
  BuzzerDriver driver(buzzer);
  Millis now = 0;

  driver.play(Buzz_Cutoff, now);
  // loop() calls this every iteration; restarting each time would make the
  // pattern run forever.
  for (int i = 0; i < 500; ++i) {
    now += 10;
    driver.play(Buzz_Cutoff, now);
    driver.update(now);
  }
  CHECK_FALSE(driver.isSounding());
}

TEST(the_pin_is_only_touched_on_a_real_change) {
  FakeBuzzer buzzer;
  BuzzerDriver driver(buzzer);
  Millis now = 0;

  driver.play(Buzz_PlugDetected, now);
  runUntil(driver, now, 1000);

  // One rising edge, one falling edge - not one write per loop iteration.
  CHECK_EQ(buzzer.edges(), 2);
}

TEST(switching_patterns_takes_effect_at_once) {
  FakeBuzzer buzzer;
  BuzzerDriver driver(buzzer);
  Millis now = 0;

  driver.play(Buzz_PlugDetected, now);
  runUntil(driver, now, 500);
  CHECK_FALSE(driver.isSounding());

  driver.play(Buzz_Fault, now);
  CHECK(driver.isSounding());
  CHECK_EQ(driver.current(), Buzz_Fault);
}
