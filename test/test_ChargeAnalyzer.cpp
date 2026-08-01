// Tests for the full-charge detection algorithm.
#include "../SmartSocket/src/core/ChargeAnalyzer.h"

#include "../SmartSocket/src/core/Config.h"
#include "TestFramework.h"

using namespace smartsocket;

namespace {

const Millis Step = config::SampleIntervalMs;

// Feeds a constant current for `durationMs` and returns the first non-Continue
// verdict, or Verdict_Continue if the whole span passed uneventfully.
ChargeVerdict feed(ChargeAnalyzer& a, Millis& now, Milliamps ma,
                   Millis durationMs) {
  const Millis end = now + durationMs;
  while (now < end) {
    now += Step;
    const ChargeVerdict v = a.update(now, ma);
    if (v != Verdict_Continue) {
      return v;
    }
  }
  return Verdict_Continue;
}

}  // namespace

TEST(taper_is_confirmed_only_after_a_full_laptop_charge) {
  ChargeAnalyzer a;
  Millis now = 1000;
  a.beginSession(now, config::TaperRatioDefaultPct);

  // A laptop pulls ~2 A while filling.
  CHECK_EQ(feed(a, now, 2000, 30u * 60u * 1000u), Verdict_Continue);
  CHECK_NEAR(a.peakMa(), 2000, 30);

  // Threshold tracks the peak, not a hardcoded number: 30% learned, lifted by the
  // 130% margin, gives ~39% of 2 A.
  CHECK_NEAR(a.taperThresholdMa(), 780, 40);

  // Battery fills; current tapers to a trickle and stays there.
  const ChargeVerdict v = feed(a, now, 300, config::TaperConfirmMs + 5000);
  CHECK_EQ(v, Verdict_Full);
  CHECK(a.cutoffCurrentMa() > 0);
}

TEST(threshold_scales_to_a_phone_not_just_a_laptop) {
  ChargeAnalyzer a;
  Millis now = 0;
  a.beginSession(now, config::TaperRatioDefaultPct);

  // A phone peaks around 500 mA - a quarter of the laptop above.
  feed(a, now, 500, 5u * 60u * 1000u);
  CHECK_NEAR(a.peakMa(), 500, 20);

  // ~39% of 500 = ~195 mA, above the 115 mA floor, so the peak governs. The same
  // code gave ~780 mA for the laptop: no configuration, just the peak.
  CHECK_NEAR(a.taperThresholdMa(), 195, 25);
}

TEST(threshold_never_sinks_below_the_noise_floor) {
  ChargeAnalyzer a;
  Millis now = 0;
  a.beginSession(now, config::TaperRatioDefaultPct);

  // A tiny 200 mA peak would imply a 98 mA threshold, which is under two ADC
  // counts clear of the noise floor. The absolute floor must take over.
  feed(a, now, 200, 60u * 1000u);
  CHECK_EQ(a.taperThresholdMa(), config::TaperFloorMa);
}

// The failure that would ruin the product: cutting a laptop's power mid-charge
// because the current dipped for a moment.
TEST(a_momentary_dip_mid_charge_does_not_cut_power) {
  ChargeAnalyzer a;
  Millis now = 0;
  a.beginSession(now, config::TaperRatioDefaultPct);

  feed(a, now, 2000, 20u * 60u * 1000u);

  // Current collapses for 30 s - well short of the 90 s confirmation window.
  CHECK_EQ(feed(a, now, 200, 30u * 1000u), Verdict_Continue);

  // ...then recovers. The device was never full.
  CHECK_EQ(feed(a, now, 2000, 10u * 60u * 1000u), Verdict_Continue);
}

TEST(repeated_dips_never_accumulate_into_a_cutoff) {
  ChargeAnalyzer a;
  Millis now = 0;
  a.beginSession(now, config::TaperRatioDefaultPct);
  feed(a, now, 2000, 5u * 60u * 1000u);

  // Ten 60-second dips total ten minutes below threshold. If the confirmation
  // window accumulated instead of resetting, this would trip a false cutoff.
  for (int i = 0; i < 10; ++i) {
    CHECK_EQ(feed(a, now, 200, 60u * 1000u), Verdict_Continue);
    CHECK_EQ(feed(a, now, 2000, 60u * 1000u), Verdict_Continue);
  }
}

TEST(an_already_full_device_is_not_cut_off_before_min_charge_time) {
  ChargeAnalyzer a;
  Millis now = 0;
  a.beginSession(now, config::TaperRatioDefaultPct);

  // Plugged in already full: current is low from the very first sample. The
  // MinChargeMs guard must hold power on until a real peak could be established.
  CHECK_EQ(feed(a, now, 200, config::MinChargeMs - 5000), Verdict_Continue);
}

TEST(unplugging_is_reported_as_unplugged_not_as_full) {
  ChargeAnalyzer a;
  Millis now = 0;
  a.beginSession(now, config::TaperRatioDefaultPct);
  feed(a, now, 2000, 10u * 60u * 1000u);

  // Current goes to zero, not to a trickle. That is a removal, and re-arming is
  // right; latching a "full" cutoff would be wrong.
  const ChargeVerdict v = feed(a, now, 0, config::UnplugConfirmMs + 30000);
  CHECK_EQ(v, Verdict_Unplugged);
}

TEST(a_brief_dropout_does_not_count_as_unplugging) {
  ChargeAnalyzer a;
  Millis now = 0;
  a.beginSession(now, config::TaperRatioDefaultPct);
  feed(a, now, 2000, 10u * 60u * 1000u);

  // Under UnplugConfirmMs (3 s), so the session must survive.
  CHECK_EQ(feed(a, now, 0, 1000), Verdict_Continue);
  CHECK_EQ(feed(a, now, 2000, 60u * 1000u), Verdict_Continue);
}

TEST(overcurrent_is_reported_immediately) {
  ChargeAnalyzer a;
  Millis now = 0;
  a.beginSession(now, config::TaperRatioDefaultPct);

  // Beyond the ACS712's range: a wiring fault, not a real load. No smoothing
  // delay is acceptable here.
  CHECK_EQ(a.update(now + Step, config::ImplausibleMa + 500), Verdict_Overcurrent);
}

TEST(sustained_overcurrent_trips_through_the_filter) {
  ChargeAnalyzer a;
  Millis now = 0;
  a.beginSession(now, config::TaperRatioDefaultPct);

  CHECK_EQ(feed(a, now, 4800, 10u * 1000u), Verdict_Overcurrent);
}

// Regression test for a bug that silently killed the entire product after about
// six charges.
//
// The learned ratio comes from the current at which a cutoff happened, which is
// always below the threshold that caused it. Fed straight back, the threshold
// ratchets down every session until it converges onto the device's trickle
// current - at which point "current < threshold" can never be true again and the
// socket never cuts off, forever, with nothing on the display to say so.
//
// A single-session test cannot see this. That is the point of this one.
TEST(learning_converges_and_keeps_working_over_many_sessions) {
  const Milliamps peak = 2000;
  const Milliamps trickle = 400;  // this device's real taper: 20% of peak

  uint16_t learned = config::TaperRatioDefaultPct;

  for (int session = 0; session < 40; ++session) {
    ChargeAnalyzer a;
    Millis now = 0;
    a.beginSession(now, learned);

    feed(a, now, peak, 20u * 60u * 1000u);
    const ChargeVerdict v = feed(a, now, trickle, config::TaperConfirmMs + 30000);

    // The socket must still be cutting off on session 40, not just session 1.
    CHECK_EQ(v, Verdict_Full);
    CHECK(a.taperThresholdMa() > trickle);

    // Same EMA the controller applies.
    uint16_t observed = a.observedTaperPct();
    if (observed < config::TaperRatioMinPct) {
      observed = config::TaperRatioMinPct;
    } else if (observed > config::TaperRatioMaxPct) {
      observed = config::TaperRatioMaxPct;
    }
    const uint32_t w = config::LearningWeightPct;
    learned = static_cast<uint16_t>(
        ((static_cast<uint32_t>(learned) * (100u - w)) + (observed * w)) / 100u);
  }

  // It should have learned this device's actual ~20% taper, not collapsed to the
  // floor.
  CHECK_NEAR(learned, 20, 5);
}

TEST(the_threshold_stays_above_the_learned_taper_ratio) {
  // The margin between "what was learned" and "where we cut" is what gives the
  // learning loop a stable fixed point instead of an absorbing one.
  ChargeAnalyzer a;
  Millis now = 0;
  a.beginSession(now, 20);
  feed(a, now, 2000, 60u * 1000u);

  CHECK(a.effectiveTaperPct() > 20);
  CHECK(a.taperThresholdMa() > (2000 * 20) / 100);
}

TEST(the_effective_ratio_is_clamped_even_after_the_margin) {
  // The upper clamp is a safety limit: a threshold at a high fraction of peak
  // would cut a laptop off partway through a normal charge.
  ChargeAnalyzer a;
  Millis now = 0;
  a.beginSession(now, config::TaperRatioMaxPct);
  feed(a, now, 2000, 60u * 1000u);

  CHECK_EQ(a.effectiveTaperPct(), config::TaperRatioMaxPct);
}

TEST(a_device_that_never_draws_a_real_charging_current_is_cut_off) {
  // A full device left plugged in while the user re-arms: its own trickle becomes
  // the session peak, so the taper threshold would be permanently out of reach
  // and it would trickle-charge forever. It must be cut instead.
  ChargeAnalyzer a;
  Millis now = 0;
  a.beginSession(now, config::TaperRatioDefaultPct);

  // Derived, not hardcoded: any load inside the PlugDetectMa..MinSessionPeakMa
  // band is by definition "present but never charging". A literal here silently
  // stops testing what it names as soon as either constant moves.
  const Milliamps trickle =
      (config::PlugDetectMa + config::MinSessionPeakMa) / 2;

  const ChargeVerdict v = feed(a, now, trickle, config::MinChargeMs + 10000);
  CHECK_EQ(v, Verdict_Full);
}

TEST(a_real_charge_is_never_mistaken_for_an_already_full_device) {
  ChargeAnalyzer a;
  Millis now = 0;
  a.beginSession(now, config::TaperRatioDefaultPct);

  // Comfortably above MinSessionPeakMa: this must charge normally, not be cut.
  CHECK_EQ(feed(a, now, 600, config::MinChargeMs + 60000), Verdict_Continue);
}

TEST(a_slow_ramping_charger_is_given_time_before_being_judged) {
  ChargeAnalyzer a;
  Millis now = 0;
  a.beginSession(now, config::TaperRatioDefaultPct);

  // Some chargers negotiate power and start low. Deciding "already full" from the
  // first seconds would cut them off before they ever ramped up.
  CHECK_EQ(feed(a, now, 200, 20u * 1000u), Verdict_Continue);
  CHECK_EQ(feed(a, now, 2000, config::MinChargeMs + 60000), Verdict_Continue);
}

TEST(observed_taper_percentage_feeds_the_learning) {
  ChargeAnalyzer a;
  Millis now = 0;
  a.beginSession(now, config::TaperRatioDefaultPct);

  feed(a, now, 2000, 20u * 60u * 1000u);
  const ChargeVerdict v = feed(a, now, 400, config::TaperConfirmMs + 10000);
  CHECK_EQ(v, Verdict_Full);

  // Cut at ~400 mA against a ~2000 mA peak = ~20%.
  CHECK_NEAR(a.observedTaperPct(), 20, 4);
}

TEST(a_learned_ratio_changes_where_the_cut_happens) {
  // Same charge curve, two learned ratios: the higher ratio must cut earlier.
  ChargeAnalyzer low;
  Millis nowLow = 0;
  low.beginSession(nowLow, 15);
  feed(low, nowLow, 2000, 10u * 60u * 1000u);

  ChargeAnalyzer high;
  Millis nowHigh = 0;
  high.beginSession(nowHigh, 50);
  feed(high, nowHigh, 2000, 10u * 60u * 1000u);

  // 15% -> ~19% after margin -> ~390 mA;  50% -> clamped to 60% -> 1200 mA.
  CHECK(high.taperThresholdMa() > low.taperThresholdMa());
  CHECK_NEAR(low.taperThresholdMa(), 390, 40);
  CHECK_NEAR(high.taperThresholdMa(), 1200, 60);
}

TEST(begin_session_clamps_an_absurd_learned_ratio) {
  ChargeAnalyzer a;
  Millis now = 0;

  // A corrupted 99% ratio would cut power almost immediately. Clamp it.
  a.beginSession(now, 99);
  feed(a, now, 2000, 60u * 1000u);
  CHECK_NEAR(a.taperThresholdMa(), (2000 * config::TaperRatioMaxPct) / 100, 60);
}

TEST(the_peak_is_not_dragged_down_by_filter_warmup) {
  ChargeAnalyzer a;
  Millis now = 0;
  a.beginSession(now, config::TaperRatioDefaultPct);

  // The filter is seeded with the first real sample. Were it to ramp from zero,
  // the early peak would read far too low and the threshold with it.
  a.update(now + Step, 2000);
  CHECK_EQ(a.smoothedMa(), 2000);
  CHECK_EQ(a.peakMa(), 2000);
}
