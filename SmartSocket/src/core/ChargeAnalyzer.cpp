#include "ChargeAnalyzer.h"

#include "Config.h"

namespace smartsocket {

ChargeAnalyzer::ChargeAnalyzer()
    : taperRatioPct_(config::TaperRatioDefaultPct),
      sessionStartMs_(0),
      belowThresholdSinceMs_(0),
      unplugSinceMs_(0),
      smoothed_(0),
      peak_(0),
      cutoffCurrentMa_(0),
      belowThreshold_(false),
      maybeUnplugged_(false),
      smoothingPrimed_(false) {}

void ChargeAnalyzer::beginSession(Millis nowMs, uint16_t taperRatioPct) {
  if (taperRatioPct < config::TaperRatioMinPct) {
    taperRatioPct = config::TaperRatioMinPct;
  } else if (taperRatioPct > config::TaperRatioMaxPct) {
    taperRatioPct = config::TaperRatioMaxPct;
  }

  taperRatioPct_ = taperRatioPct;
  sessionStartMs_ = nowMs;
  belowThresholdSinceMs_ = nowMs;
  unplugSinceMs_ = nowMs;
  smoothed_ = 0;
  peak_ = 0;
  cutoffCurrentMa_ = 0;
  belowThreshold_ = false;
  maybeUnplugged_ = false;
  smoothingPrimed_ = false;
}

uint16_t ChargeAnalyzer::effectiveTaperPct() const {
  uint32_t eff =
      (static_cast<uint32_t>(taperRatioPct_) * config::TaperMarginPct) / 100u;

  // The upper clamp is a safety limit, not a formality: a threshold at a high
  // fraction of peak would cut a laptop's power partway through a normal charge.
  if (eff > config::TaperRatioMaxPct) {
    eff = config::TaperRatioMaxPct;
  } else if (eff < config::TaperRatioMinPct) {
    eff = config::TaperRatioMinPct;
  }
  return static_cast<uint16_t>(eff);
}

Milliamps ChargeAnalyzer::taperThresholdMa() const {
  const Milliamps fromPeak =
      (peak_ * static_cast<Milliamps>(effectiveTaperPct())) / 100;
  // Never let the threshold sink into the noise floor, however small the peak.
  return fromPeak > config::TaperFloorMa ? fromPeak : config::TaperFloorMa;
}

uint16_t ChargeAnalyzer::observedTaperPct() const {
  if (peak_ <= 0) {
    return 0;
  }
  return static_cast<uint16_t>((cutoffCurrentMa_ * 100) / peak_);
}

ChargeVerdict ChargeAnalyzer::update(Millis nowMs, Milliamps rawMa) {
  // A reading beyond the sensor's physical range means the wiring or the sensor is
  // broken, not that the load is enormous. Report it before it feeds the filter.
  if (rawMa > config::ImplausibleMa || rawMa < -config::ImplausibleMa) {
    return Verdict_Overcurrent;
  }

  if (rawMa < 0) {
    rawMa = 0;
  }

  // Seed the filter with the first real sample instead of ramping up from zero,
  // which would otherwise suppress the peak for the first few seconds.
  if (!smoothingPrimed_) {
    smoothed_ = rawMa;
    smoothingPrimed_ = true;
  } else {
    smoothed_ += ((rawMa - smoothed_) * config::CurrentSmoothingPct) / 100;
  }

  if (smoothed_ > config::OvercurrentMa) {
    return Verdict_Overcurrent;
  }

  if (smoothed_ > peak_) {
    peak_ = smoothed_;
  }

  // --- unplug detection ------------------------------------------------------
  // An unplug and a full charge both look like "low current". They are told apart
  // by degree: gone entirely vs. merely tapered to a trickle.
  if (smoothed_ < config::UnplugMa) {
    if (!maybeUnplugged_) {
      maybeUnplugged_ = true;
      unplugSinceMs_ = nowMs;
    } else if (nowMs - unplugSinceMs_ >= config::UnplugConfirmMs) {
      return Verdict_Unplugged;
    }
  } else {
    maybeUnplugged_ = false;
  }

  // --- taper detection -------------------------------------------------------
  const Milliamps threshold = taperThresholdMa();

  if (smoothed_ < threshold) {
    if (!belowThreshold_) {
      belowThreshold_ = true;
      belowThresholdSinceMs_ = nowMs;
    }
  } else {
    // Any excursion back above the threshold restarts the confirmation window.
    // This is what makes a momentary mid-charge dip harmless.
    belowThreshold_ = false;
  }

  const bool chargedLongEnough =
      (nowMs - sessionStartMs_) >= config::MinChargeMs;

  // A load that never rose to a real charging current is a device that was
  // already full - typically one left plugged in while the user re-armed the
  // socket. Its own trickle becomes the session peak, which would put the taper
  // threshold permanently out of reach and leave it trickling forever. Cut it.
  //
  // This waits out MinChargeMs rather than deciding immediately, so a charger
  // that negotiates power slowly still gets time to ramp up.
  if (chargedLongEnough && peak_ < config::MinSessionPeakMa) {
    cutoffCurrentMa_ = smoothed_;
    return Verdict_Full;
  }

  // Both guards must hold before power is cut:
  //  1. the session ran long enough to establish a real peak, and
  //  2. the current stayed down continuously, not just instantaneously.
  const bool taperConfirmed =
      belowThreshold_ &&
      (nowMs - belowThresholdSinceMs_) >= config::TaperConfirmMs;

  if (chargedLongEnough && taperConfirmed) {
    cutoffCurrentMa_ = smoothed_;
    return Verdict_Full;
  }

  return Verdict_Continue;
}

}  // namespace smartsocket
