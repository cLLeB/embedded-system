// ChargeAnalyzer.h - decides when a charging device has reached full charge.
//
// This is the heart of the product. It is a pure decision function: given a stream
// of (current, timestamp) samples it returns a verdict. It owns no hardware, reads
// no clock of its own, and has no side effects, which makes every branch below
// reachable from a native test in microseconds.
//
// Portable C++: must not include Arduino.h.
#ifndef SMARTSOCKET_CORE_CHARGEANALYZER_H
#define SMARTSOCKET_CORE_CHARGEANALYZER_H

#include "Types.h"

namespace smartsocket {

class ChargeAnalyzer {
 public:
  ChargeAnalyzer();

  // Begins a charge session. `taperRatioPct` is the learned (or default) threshold
  // as a percentage of the session's observed peak.
  void beginSession(Millis nowMs, uint16_t taperRatioPct);

  // Feeds one sample. Must be called with monotonically non-decreasing timestamps.
  ChargeVerdict update(Millis nowMs, Milliamps rawMa);

  // --- session observations, valid after beginSession() ---

  Milliamps smoothedMa() const { return smoothed_; }
  Milliamps peakMa() const { return peak_; }

  // The current level below which the device is considered full. Derived from the
  // peak, so it self-scales to a phone or a laptop with no configuration.
  Milliamps taperThresholdMa() const;

  // The learned ratio with config::TaperMarginPct applied and the result clamped.
  // This, not the raw learned ratio, is what decides the threshold - see the
  // comment on TaperMarginPct for why the two must differ.
  uint16_t effectiveTaperPct() const;

  Millis elapsedMs(Millis nowMs) const { return nowMs - sessionStartMs_; }

  // Current at the moment the Full verdict was returned. Feeds the learning step.
  Milliamps cutoffCurrentMa() const { return cutoffCurrentMa_; }

  // The taper ratio this session actually exhibited, as a percentage of peak.
  // Only meaningful after a Full verdict. Returns 0 if the peak was zero.
  uint16_t observedTaperPct() const;

 private:
  uint16_t taperRatioPct_;
  Millis sessionStartMs_;
  Millis belowThresholdSinceMs_;
  Millis unplugSinceMs_;
  Milliamps smoothed_;
  Milliamps peak_;
  Milliamps cutoffCurrentMa_;
  bool belowThreshold_;
  bool maybeUnplugged_;
  bool smoothingPrimed_;
};

}  // namespace smartsocket

#endif  // SMARTSOCKET_CORE_CHARGEANALYZER_H
