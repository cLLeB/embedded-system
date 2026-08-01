#include "BuzzerDriver.h"

namespace smartsocket {
namespace {

// Pattern shapes, all expressed as functions of elapsed time so that no state
// beyond a start timestamp is needed.
const Millis ChirpMs = 80;

const Millis BeepCycleMs = 400;   // 200 ms on, 200 ms off
const Millis BeepOnMs = 200;
const Millis BurstMs = 1200;      // three beeps
const Millis BurstGapMs = 600;
const Millis CutoffTotalMs = BurstMs + BurstGapMs + BurstMs;

const Millis FaultCycleMs = 1000;  // 500 ms on, 500 ms off, forever
const Millis FaultOnMs = 500;

bool cutoffSounding(Millis elapsed) {
  if (elapsed >= CutoffTotalMs) {
    return false;
  }
  if (elapsed >= BurstMs && elapsed < BurstMs + BurstGapMs) {
    return false;  // the pause between bursts
  }
  const Millis intoBurst =
      (elapsed < BurstMs) ? elapsed : (elapsed - BurstMs - BurstGapMs);
  return (intoBurst % BeepCycleMs) < BeepOnMs;
}

}  // namespace

BuzzerDriver::BuzzerDriver(IBuzzer& buzzer)
    : buzzer_(buzzer),
      pattern_(Buzz_Silent),
      patternStartMs_(0),
      sounding_(false) {
  apply(false);
}

void BuzzerDriver::apply(bool sounding) {
  // Only touch the pin on an actual change; the HAL write is cheap but this keeps
  // the fake's call log meaningful in tests.
  if (sounding != sounding_) {
    sounding_ = sounding;
    buzzer_.setSounding(sounding);
  }
}

void BuzzerDriver::play(BuzzerPattern pattern, Millis nowMs) {
  if (pattern == pattern_) {
    return;  // idempotent: safe to call every loop
  }
  pattern_ = pattern;
  patternStartMs_ = nowMs;
  update(nowMs);
}

void BuzzerDriver::silence(Millis nowMs) {
  pattern_ = Buzz_Silent;
  patternStartMs_ = nowMs;
  apply(false);
}

void BuzzerDriver::update(Millis nowMs) {
  const Millis elapsed = nowMs - patternStartMs_;

  switch (pattern_) {
    case Buzz_PlugDetected:
      apply(elapsed < ChirpMs);
      break;

    case Buzz_Cutoff:
      apply(cutoffSounding(elapsed));
      break;

    case Buzz_Fault:
      apply((elapsed % FaultCycleMs) < FaultOnMs);
      break;

    case Buzz_Silent:
    default:
      apply(false);
      break;
  }
}

}  // namespace smartsocket
