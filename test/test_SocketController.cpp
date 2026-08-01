// Tests for the state machine that owns the relay.
#include "../SmartSocket/src/core/SocketController.h"

#include "../SmartSocket/src/core/Config.h"
#include "Fakes.h"
#include "TestFramework.h"

using namespace smartsocket;
using namespace smartsocket::fakes;

namespace {

// Bundles the controller with its fakes so each test reads as a scenario rather
// than as wiring.
struct Rig {
  FakeClock clock;
  FakeCurrentSensor sensor;
  FakeRelay relay;
  FakeProfileStore store;
  SocketController controller;

  Rig() : controller(clock, sensor, relay, store), stuckRelay(false) {}

  // The ACS712 sits DOWNSTREAM of the relay, so an open relay means the sensor
  // reads zero whatever is plugged in. Modelling that here rather than holding
  // the current constant is what makes the relay-verification tests mean
  // anything: a welded contact is precisely the case where this stops being
  // true, and `stuckRelay` is how a test asks for it.
  bool stuckRelay;

  // Advances time in sampling-sized steps, with `ma` offered to the socket.
  void run(Milliamps ma, Millis durationMs) {
    const Millis end = clock.now() + durationMs;
    while (clock.now() < end) {
      sensor.setCurrent((relay.isClosed() || stuckRelay) ? ma : 0);
      clock.advance(config::SampleIntervalMs);
      controller.update();
    }
  }

  // Plug in a device and get all the way to Charging.
  void startCharging(Milliamps ma) {
    run(ma, config::SettleMs + (4 * config::SampleIntervalMs));
  }

  // Advances until `want` is reached, or `limitMs` runs out. Returns whether it
  // arrived.
  //
  // Necessary because run() overshoots: it advances a fixed duration, so a state
  // the machine passes through briefly - Probing lasts only ProbeDurationMs - is
  // entered and left inside a single call, and asserting afterwards sees the
  // state on the far side of it.
  bool runUntil(Milliamps ma, SocketState want, Millis limitMs) {
    const Millis end = clock.now() + limitMs;
    while (controller.state() != want && clock.now() < end) {
      sensor.setCurrent((relay.isClosed() || stuckRelay) ? ma : 0);
      clock.advance(config::SampleIntervalMs);
      controller.update();
    }
    return controller.state() == want;
  }
};

// A load that is present but is not a charge: inside the
// PlugDetectMa..MinSessionPeakMa band by construction. Derived rather than
// hardcoded, because a literal here silently stops testing what it names as
// soon as either constant moves - which is exactly what happened when these
// thresholds were rescaled from the 3.7 V rig to 230 V mains.
const Milliamps TrickleMa =
    (config::PlugDetectMa + config::MinSessionPeakMa) / 2;

}  // namespace

TEST(boot_opens_the_relay_before_anything_else) {
  Rig rig;
  rig.controller.begin();

  // Calibration is only valid with zero current, and a reset mid-charge must not
  // leave the load energized.
  CHECK_EQ(rig.sensor.calibrateCalls(), 1);
  CHECK(rig.relay.history().size() >= 1);
  CHECK_FALSE(rig.relay.history()[0]);
}

TEST(idle_keeps_power_on_so_a_plug_in_can_be_seen) {
  Rig rig;
  rig.controller.begin();

  // An open relay would make the socket blind: current is the only plug-in signal.
  CHECK_EQ(rig.controller.state(), State_Idle);
  CHECK(rig.relay.isClosed());
}

TEST(plugging_in_moves_through_settling_to_charging) {
  Rig rig;
  rig.controller.begin();

  rig.run(2000, 2 * config::SampleIntervalMs);
  CHECK_EQ(rig.controller.state(), State_Settling);

  rig.run(2000, config::SettleMs);
  CHECK_EQ(rig.controller.state(), State_Charging);
  CHECK(rig.relay.isClosed());
}

TEST(inrush_during_settling_does_not_poison_the_peak) {
  Rig rig;
  rig.controller.begin();

  // A charger's inrush spike, then its real steady draw. Sampling the spike as
  // the peak would push the taper threshold far too high and cut power early.
  rig.run(4000, config::SettleMs - config::SampleIntervalMs);
  rig.startCharging(1000);
  rig.run(1000, 60u * 1000u);

  CHECK_EQ(rig.controller.state(), State_Charging);
  CHECK(rig.controller.status().peakMa < 2000);
}

TEST(a_full_charge_cuts_the_relay_open) {
  Rig rig;
  rig.controller.begin();
  rig.startCharging(2000);
  rig.run(2000, 20u * 60u * 1000u);

  rig.run(300, config::TaperConfirmMs + 10000);

  CHECK_EQ(rig.controller.state(), State_Cutoff);
  CHECK_FALSE(rig.relay.isClosed());
}

TEST(cutoff_never_leaves_power_on_however_long_it_waits) {
  Rig rig;
  rig.controller.begin();
  rig.startCharging(2000);
  rig.run(2000, 20u * 60u * 1000u);
  rig.run(300, config::TaperConfirmMs + 10000);
  CHECK_EQ(rig.controller.state(), State_Cutoff);

  // The socket probes periodically rather than latching forever, but a probe is
  // not auto-rearming: over an hour with nothing drawing, it must never come to
  // rest with power on. Cutoff and Probing are the only states allowed here -
  // reaching Idle or Charging would mean it had re-armed itself, which is exactly
  // the trickle charging this product exists to prevent.
  for (uint8_t minute = 0; minute < 60; ++minute) {
    rig.run(0, 60u * 1000u);
    const SocketState s = rig.controller.state();
    CHECK(s == State_Cutoff || s == State_Probing);
  }
}

TEST(a_button_press_clears_cutoff_and_re_arms) {
  Rig rig;
  rig.controller.begin();
  rig.startCharging(2000);
  rig.run(2000, 20u * 60u * 1000u);
  rig.run(300, config::TaperConfirmMs + 10000);
  CHECK_EQ(rig.controller.state(), State_Cutoff);

  rig.controller.onButton(Button_Action, ButtonEvent_ShortPress);
  CHECK_EQ(rig.controller.state(), State_Idle);
  CHECK(rig.relay.isClosed());
}

TEST(unplugging_re_arms_without_latching) {
  Rig rig;
  rig.controller.begin();
  rig.startCharging(2000);
  rig.run(2000, 5u * 60u * 1000u);

  rig.run(0, config::UnplugConfirmMs + 5000);

  CHECK_EQ(rig.controller.state(), State_Idle);
  CHECK(rig.relay.isClosed());
}

TEST(overcurrent_faults_and_opens_the_relay) {
  Rig rig;
  rig.controller.begin();
  rig.startCharging(1000);

  rig.run(config::ImplausibleMa + 1000, 2 * config::SampleIntervalMs);

  CHECK_EQ(rig.controller.state(), State_Fault);
  CHECK_FALSE(rig.relay.isClosed());
}

TEST(a_tap_clears_a_fault) {
  Rig rig;
  rig.controller.begin();
  rig.startCharging(1000);
  rig.run(config::ImplausibleMa + 1000, 2 * config::SampleIntervalMs);
  CHECK_EQ(rig.controller.state(), State_Fault);

  rig.controller.onButton(Button_Action, ButtonEvent_ShortPress);
  CHECK_EQ(rig.controller.state(), State_Idle);
}

TEST(an_over_limit_load_faults_in_idle_without_waiting_to_reach_charging) {
  Rig rig;
  rig.controller.begin();
  CHECK_EQ(rig.controller.state(), State_Idle);

  // A ~5 A heater on 230 V: over the fault limit, under the implausible limit.
  // Checking the limit only inside the analyzer would leave the contacts closed
  // for the ~5.5 s it takes to cross Idle and Settling first.
  rig.run(config::OvercurrentMa + 200, 2 * config::SampleIntervalMs);

  CHECK_EQ(rig.controller.state(), State_Fault);
  CHECK_FALSE(rig.relay.isClosed());
}

TEST(an_over_limit_load_faults_within_one_sample_not_one_filter_time_constant) {
  Rig rig;
  rig.controller.begin();
  rig.startCharging(1000);

  // Tested against the raw reading, so it trips at once. Against the smoothed
  // value the EMA would take seconds to climb past the limit - and a protection
  // limit that waits for a filter is not protection.
  rig.sensor.setCurrent(config::OvercurrentMa + 100);
  rig.clock.advance(config::SampleIntervalMs);
  rig.controller.update();

  CHECK_EQ(rig.controller.state(), State_Fault);
  CHECK_FALSE(rig.relay.isClosed());
}

TEST(re_arming_with_a_full_device_still_plugged_in_cuts_it_again) {
  Rig rig;
  rig.controller.begin();
  rig.startCharging(2000);
  rig.run(2000, 20u * 60u * 1000u);
  rig.run(300, config::TaperConfirmMs + 10000);
  CHECK_EQ(rig.controller.state(), State_Cutoff);

  // The user taps ACTION without unplugging. The relay closes and the full device
  // resumes its trickle. That trickle would become the new session's peak, making
  // the taper threshold unreachable and trickle-charging it forever - the exact
  // wear this product exists to prevent, re-entered by one button press.
  rig.controller.onButton(Button_Action, ButtonEvent_ShortPress);
  CHECK_EQ(rig.controller.state(), State_Idle);

  rig.run(TrickleMa, config::MinChargeMs + 30000);

  CHECK_EQ(rig.controller.state(), State_Cutoff);
  CHECK_FALSE(rig.relay.isClosed());
}

TEST(an_already_full_session_does_not_poison_the_learned_ratio) {
  Rig rig;
  rig.controller.begin();
  const uint16_t before = rig.controller.status().taperRatioPct;

  // This session's taper ratio is ~100%, which is meaningless as a device
  // characteristic. Learning from it would drag the stored ratio up and start
  // cutting real charges off early.
  rig.run(TrickleMa, config::SettleMs + config::MinChargeMs + 30000);
  CHECK_EQ(rig.controller.state(), State_Cutoff);

  CHECK_EQ(rig.controller.status().taperRatioPct, before);
}

TEST(idle_does_not_report_the_previous_sessions_peak) {
  Rig rig;
  rig.controller.begin();
  rig.startCharging(2000);
  rig.run(2000, 5u * 60u * 1000u);
  CHECK(rig.controller.status().peakMa > 1000);

  rig.run(0, config::UnplugConfirmMs + 5000);
  CHECK_EQ(rig.controller.state(), State_Idle);

  // Showing the last charge's figures on an idle socket presents them as live.
  CHECK_EQ(rig.controller.status().peakMa, 0);
  CHECK_EQ(rig.controller.status().thresholdMa, 0);
}

TEST(long_press_toggles_manual_power_off) {
  Rig rig;
  rig.controller.begin();

  rig.controller.onButton(Button_Action, ButtonEvent_LongPress);
  CHECK_EQ(rig.controller.state(), State_ManualOff);
  CHECK_FALSE(rig.relay.isClosed());

  rig.controller.onButton(Button_Action, ButtonEvent_LongPress);
  CHECK_EQ(rig.controller.state(), State_Idle);
  CHECK(rig.relay.isClosed());
}

TEST(manual_off_ignores_a_device_being_plugged_in) {
  Rig rig;
  rig.controller.begin();
  rig.controller.onButton(Button_Action, ButtonEvent_LongPress);

  rig.run(2000, 60u * 1000u);

  CHECK_EQ(rig.controller.state(), State_ManualOff);
  CHECK_FALSE(rig.relay.isClosed());
}

TEST(a_completed_session_is_persisted) {
  Rig rig;
  rig.controller.begin();
  rig.startCharging(2000);
  rig.run(2000, 20u * 60u * 1000u);
  rig.run(400, config::TaperConfirmMs + 10000);

  CHECK_EQ(rig.controller.state(), State_Cutoff);
  CHECK_EQ(rig.store.saveCalls(), 1);
  CHECK_EQ(rig.controller.status().cutoffCount, 1);
}

TEST(the_learned_ratio_moves_toward_what_the_device_actually_does) {
  Rig rig;
  rig.controller.begin();
  const uint16_t before = rig.controller.status().taperRatioPct;

  // Cut at ~400/2000 = 20%, below the 38% default, so learning should pull the
  // stored ratio downward.
  rig.startCharging(2000);
  rig.run(2000, 20u * 60u * 1000u);
  rig.run(400, config::TaperConfirmMs + 10000);

  const uint16_t after = rig.controller.status().taperRatioPct;
  CHECK(after < before);
  CHECK(after >= config::TaperRatioMinPct);
}

TEST(learning_is_gradual_so_one_odd_session_cannot_retrain_the_socket) {
  Rig rig;
  rig.controller.begin();

  rig.startCharging(2000);
  rig.run(2000, 20u * 60u * 1000u);
  rig.run(400, config::TaperConfirmMs + 10000);

  // A single 20% session must not slam the learned value from 30% to 20%; the
  // EMA should leave it in between.
  const uint16_t after = rig.controller.status().taperRatioPct;
  CHECK(after > 20);
  CHECK(after < config::TaperRatioDefaultPct);
}

TEST(a_stored_profile_is_restored_at_boot) {
  Rig rig;
  DeviceProfile seeded;
  seeded.learnedTaperPct = 22;
  seeded.lastPeakMa = 1500;
  seeded.cutoffCount = 9;
  seeded.totalSavedMs = 123456;
  rig.store.seed(seeded);

  rig.controller.begin();

  CHECK_EQ(rig.controller.status().taperRatioPct, 22);
  CHECK_EQ(rig.controller.status().cutoffCount, 9);
}

TEST(a_corrupted_profile_falls_back_to_defaults_instead_of_acting_on_garbage) {
  Rig rig;
  DeviceProfile seeded;
  seeded.learnedTaperPct = 22;
  seeded.lastPeakMa = 1500;
  seeded.cutoffCount = 9;
  seeded.totalSavedMs = 0;
  rig.store.seed(seeded);
  rig.store.corrupt(5);

  rig.controller.begin();

  CHECK_EQ(rig.controller.status().taperRatioPct, config::TaperRatioDefaultPct);
  CHECK_EQ(rig.controller.status().cutoffCount, 0);
}

TEST(an_out_of_band_stored_ratio_is_rejected) {
  Rig rig;
  DeviceProfile seeded;
  seeded.learnedTaperPct = 95;  // valid CRC, impossible value
  seeded.lastPeakMa = 1500;
  seeded.cutoffCount = 1;
  seeded.totalSavedMs = 0;
  rig.store.seed(seeded);

  rig.controller.begin();

  CHECK_EQ(rig.controller.status().taperRatioPct, config::TaperRatioDefaultPct);
}

TEST(cutoff_emits_an_alert_then_stops_repeating_it) {
  Rig rig;
  rig.controller.begin();
  rig.startCharging(2000);
  rig.run(2000, 20u * 60u * 1000u);
  rig.run(300, config::TaperConfirmMs + 10000);

  BuzzerPattern p = Buzz_Silent;
  CHECK(rig.controller.takeAlert(p));
  CHECK_EQ(p, Buzz_Cutoff);

  // Consumed: it must not re-fire every loop or the pattern would restart forever.
  CHECK_FALSE(rig.controller.takeAlert(p));
}

TEST(clearing_a_fault_actively_commands_silence) {
  Rig rig;
  rig.controller.begin();
  rig.startCharging(1000);
  rig.run(config::ImplausibleMa + 1000, 2 * config::SampleIntervalMs);

  BuzzerPattern p = Buzz_Silent;
  CHECK(rig.controller.takeAlert(p));
  CHECK_EQ(p, Buzz_Fault);

  // The fault pattern never ends by itself, so clearing the fault has to deliver
  // an explicit silence command - otherwise the socket beeps forever.
  rig.controller.onButton(Button_Action, ButtonEvent_ShortPress);
  CHECK(rig.controller.takeAlert(p));
  CHECK_EQ(p, Buzz_Silent);
}

TEST(time_saved_survives_a_power_cycle) {
  Rig rig;
  rig.controller.begin();
  rig.startCharging(2000);
  rig.run(2000, 10u * 60u * 1000u);
  rig.run(300, config::TaperConfirmMs + 10000);
  rig.run(0, 60u * 1000u);

  // Acknowledging is the moment the saved-time count is final, so it must be
  // written then - otherwise the statistic resets every time the socket reboots.
  rig.controller.onButton(Button_Action, ButtonEvent_ShortPress);
  const uint32_t saved = rig.controller.status().totalSavedMs;
  CHECK(saved > 0);

  // A fresh controller over the same store: the reboot.
  SocketController rebooted(rig.clock, rig.sensor, rig.relay, rig.store);
  rebooted.begin();
  CHECK_EQ(rebooted.status().totalSavedMs, saved);
}

TEST(saved_time_accumulates_only_while_power_is_actually_cut) {
  Rig rig;
  rig.controller.begin();
  rig.startCharging(2000);
  rig.run(2000, 10u * 60u * 1000u);
  CHECK_EQ(rig.controller.status().totalSavedMs, 0u);

  rig.run(300, config::TaperConfirmMs + 10000);
  CHECK_EQ(rig.controller.state(), State_Cutoff);

  // Measure from the moment of cutoff rather than from zero: the run above
  // overshoots the transition by a few seconds, and those seconds are genuinely
  // saved time. Taking a delta keeps this test about the accumulation rate rather
  // than about the setup's timing.
  const uint32_t atCutoff = rig.controller.status().totalSavedMs;
  rig.run(0, 60u * 1000u);
  CHECK_NEAR(rig.controller.status().totalSavedMs - atCutoff, 60000, 2000);
}

// --- automatic recovery from a cutoff --------------------------------------
//
// The sensor sits downstream of the relay, so an open relay makes the socket
// blind. These cover the probe that gets it its sight back.

namespace {

// Charge, then taper, landing in Cutoff with `finalMa` as the last reading. That
// last reading is what picks the probe interval, so tests that care about the
// interval have to control it.
void cutOffLeaving(Rig& rig, Milliamps finalMa) {
  rig.controller.begin();
  rig.startCharging(2000);
  rig.run(2000, 20u * 60u * 1000u);
  rig.run(finalMa, config::TaperConfirmMs + 10000);
}

// Generous, because the point of these tests is which interval was chosen, not
// its exact value.
const Millis WaitSlackMs = 60u * 1000u;

}  // namespace

TEST(a_probe_that_finds_a_real_load_resumes_charging) {
  Rig rig;
  cutOffLeaving(rig, 300);
  CHECK_EQ(rig.controller.state(), State_Cutoff);

  // Nothing is plugged in, so the socket waits, then looks. Without the probe
  // this needs a button press; with it, the socket finds a new device on its own.
  CHECK(rig.runUntil(0, State_Probing,
                     config::ProbeChargedIntervalMs + WaitSlackMs));
  CHECK(rig.relay.isClosed());

  rig.run(2000, config::SettleMs + (4 * config::SampleIntervalMs));

  CHECK_EQ(rig.controller.state(), State_Charging);
  CHECK(rig.relay.isClosed());
}

TEST(a_probe_that_finds_nothing_opens_the_relay_again) {
  Rig rig;
  cutOffLeaving(rig, 300);

  CHECK(rig.runUntil(0, State_Probing,
                     config::ProbeChargedIntervalMs + WaitSlackMs));

  rig.run(0, config::ProbeDurationMs + config::SampleIntervalMs);

  CHECK_EQ(rig.controller.state(), State_Cutoff);
  CHECK_FALSE(rig.relay.isClosed());
}

TEST(a_probe_does_not_resume_on_a_load_that_is_merely_present) {
  Rig rig;
  cutOffLeaving(rig, 300);
  CHECK(rig.runUntil(0, State_Probing,
                     config::ProbeChargedIntervalMs + WaitSlackMs));

  // A device that was cut for being full still draws something while it runs.
  // Resuming on that would put the socket straight back into the trickle it just
  // escaped, so the bar is a real charging current, not merely a present load.
  const Milliamps stillRunning =
      (config::PlugDetectMa + config::ProbeResumeMa) / 2;
  rig.run(stillRunning, config::ProbeDurationMs + config::SampleIntervalMs);

  CHECK_EQ(rig.controller.state(), State_Cutoff);
  CHECK_FALSE(rig.relay.isClosed());
}

TEST(a_probe_that_finds_nothing_does_not_announce_a_second_cutoff) {
  Rig rig;
  cutOffLeaving(rig, 300);

  BuzzerPattern alert = Buzz_Silent;
  while (rig.controller.takeAlert(alert)) {
  }

  // Three beeps every fifteen minutes, all night, for one charge that finished
  // hours ago. A probe finding nothing is not a new cutoff.
  rig.run(0, config::ProbeChargedIntervalMs + config::ProbeDurationMs +
                 WaitSlackMs);

  while (rig.controller.takeAlert(alert)) {
    CHECK(alert != Buzz_Cutoff);
  }
}

TEST(a_probe_that_finds_nothing_does_not_count_as_another_cutoff) {
  Rig rig;
  cutOffLeaving(rig, 300);
  const uint16_t after = rig.controller.status().cutoffCount;

  rig.run(0, (3 * config::ProbeChargedIntervalMs) + WaitSlackMs);

  CHECK_EQ(rig.controller.status().cutoffCount, after);
}

TEST(an_empty_socket_is_probed_sooner_than_one_still_drawing) {
  // Nothing was drawing when power was cut, so the socket is probably empty.
  // Energizing an empty socket costs nothing, so it looks often enough that
  // plugging something in feels immediate rather than taking a quarter of an hour.
  Rig empty;
  cutOffLeaving(empty, config::ProbeEmptyBelowMa - 5);
  CHECK_EQ(empty.controller.state(), State_Cutoff);
  CHECK(empty.runUntil(0, State_Probing,
                       config::ProbeEmptyIntervalMs + WaitSlackMs));

  // A device still drawing at cutoff is probably still sitting there, still full.
  // Probing it at the same rate would just re-energize a full battery.
  Rig occupied;
  cutOffLeaving(occupied, 300);
  CHECK_EQ(occupied.controller.state(), State_Cutoff);
  CHECK_FALSE(occupied.runUntil(0, State_Probing,
                                config::ProbeEmptyIntervalMs * 2));
}

TEST(a_failed_probe_keeps_the_interval_the_real_cutoff_chose) {
  // The reading at the end of a probe is taken with the relay closed for a few
  // seconds, not at the end of a charge. Letting it re-pick the interval would
  // flip a full device onto the 60 s schedule and re-energize it all night.
  Rig rig;
  cutOffLeaving(rig, 300);

  CHECK(rig.runUntil(0, State_Probing,
                     config::ProbeChargedIntervalMs + WaitSlackMs));
  rig.run(0, config::ProbeDurationMs + config::SampleIntervalMs);
  CHECK_EQ(rig.controller.state(), State_Cutoff);

  CHECK_FALSE(rig.runUntil(0, State_Probing, config::ProbeEmptyIntervalMs * 2));
}

// --- relay verification ------------------------------------------------------
//
// setClosed(false) is a command, not a measurement. The ACS712 is downstream of
// the relay, so if the contacts do not open the load current still flows through
// it - which makes a welded contact detectable with no extra hardware.

TEST(current_still_flowing_after_a_cutoff_is_reported_as_a_stuck_relay) {
  Rig rig;
  cutOffLeaving(rig, 300);
  rig.stuckRelay = true;  // contacts welded: current keeps flowing
  CHECK_EQ(rig.controller.state(), State_Cutoff);

  // The contacts were commanded open and the load is still drawing. Until this
  // check existed the socket displayed "FULL - POWER CUT" over a live outlet.
  rig.run(2000, config::RelayStuckGraceMs + config::RelayStuckConfirmMs +
                    (4 * config::SampleIntervalMs));

  CHECK_EQ(rig.controller.state(), State_RelayStuck);
}

TEST(a_clean_cutoff_is_never_reported_as_a_stuck_relay) {
  Rig rig;
  cutOffLeaving(rig, 300);

  // Contacts opened, current went away. Well inside the probe interval, so
  // nothing else can move the state either.
  rig.run(0, config::ProbeChargedIntervalMs / 2);

  CHECK_EQ(rig.controller.state(), State_Cutoff);
}

TEST(the_grace_period_ignores_current_that_is_still_decaying) {
  Rig rig;
  rig.controller.begin();
  rig.startCharging(2000);
  rig.run(2000, 20u * 60u * 1000u);

  // Land exactly ON the cutoff rather than overshooting it, because this test is
  // about the first few seconds after the contacts part and nothing else.
  CHECK(rig.runUntil(300, State_Cutoff, config::TaperConfirmMs + 30000));
  rig.stuckRelay = true;

  // A load's input capacitors and the smoothing filter both take time to fall.
  // Raising a fault on that would fire on every single successful cutoff.
  rig.run(2000, config::RelayStuckGraceMs - config::SampleIntervalMs);

  CHECK_EQ(rig.controller.state(), State_Cutoff);
}

TEST(one_noisy_sample_does_not_raise_a_stuck_relay_fault) {
  Rig rig;
  cutOffLeaving(rig, 300);
  rig.run(0, config::RelayStuckGraceMs + config::SampleIntervalMs);

  // A single reading above the bar, then gone again.
  rig.sensor.setCurrent(2000);
  rig.clock.advance(config::SampleIntervalMs);
  rig.controller.update();
  rig.run(0, config::RelayStuckConfirmMs + config::SampleIntervalMs);

  CHECK_EQ(rig.controller.state(), State_Cutoff);
}

TEST(a_stuck_relay_latches_and_never_probes) {
  Rig rig;
  cutOffLeaving(rig, 300);
  rig.stuckRelay = true;  // contacts welded: current keeps flowing
  rig.run(2000, config::RelayStuckGraceMs + config::RelayStuckConfirmMs +
                    (4 * config::SampleIntervalMs));
  CHECK_EQ(rig.controller.state(), State_RelayStuck);

  // Probing closes the contacts on purpose. Doing that when they are already
  // stuck closed would be the socket cheerfully energizing a fault it has just
  // detected, and would also mask it: the probe would find current and "resume".
  rig.run(2000, (2 * config::ProbeChargedIntervalMs) + 5000);

  CHECK_EQ(rig.controller.state(), State_RelayStuck);
}

TEST(a_stuck_relay_sounds_the_fault_pattern) {
  Rig rig;
  cutOffLeaving(rig, 300);
  rig.stuckRelay = true;  // contacts welded: current keeps flowing
  BuzzerPattern alert = Buzz_Silent;
  while (rig.controller.takeAlert(alert)) {
  }

  rig.run(2000, config::RelayStuckGraceMs + config::RelayStuckConfirmMs +
                    (4 * config::SampleIntervalMs));

  CHECK(rig.controller.takeAlert(alert));
  CHECK_EQ(alert, Buzz_Fault);
}

TEST(a_tap_clears_a_stuck_relay_fault) {
  Rig rig;
  cutOffLeaving(rig, 300);
  rig.stuckRelay = true;  // contacts welded: current keeps flowing
  rig.run(2000, config::RelayStuckGraceMs + config::RelayStuckConfirmMs +
                    (4 * config::SampleIntervalMs));
  CHECK_EQ(rig.controller.state(), State_RelayStuck);

  // Clearable, because a user who has fixed the wiring must be able to carry on
  // without a power cycle. It will simply be raised again if it is still stuck.
  rig.controller.onButton(Button_Action, ButtonEvent_ShortPress);
  CHECK_EQ(rig.controller.state(), State_Idle);
}

// --- remote commands ---------------------------------------------------------
//
// A phone must not be able to reach a state the front panel cannot, so every
// remote command routes through the same transitions the buttons use. These
// pin that down.

TEST(a_remote_cut_opens_the_relay_from_charging) {
  Rig rig;
  rig.controller.begin();
  rig.startCharging(2000);
  CHECK_EQ(rig.controller.state(), State_Charging);

  rig.controller.onRemote(Remote_Cut);

  CHECK_EQ(rig.controller.state(), State_Cutoff);
  CHECK_FALSE(rig.relay.isClosed());
}

TEST(a_remote_cut_while_already_cut_changes_nothing) {
  Rig rig;
  cutOffLeaving(rig, 300);
  const uint16_t before = rig.controller.status().cutoffCount;

  // Re-entering Cutoff would restart the probe timer and re-announce a cutoff
  // that already happened. "Cut" when already cut is a no-op, not an error.
  rig.controller.onRemote(Remote_Cut);

  CHECK_EQ(rig.controller.state(), State_Cutoff);
  CHECK_EQ(rig.controller.status().cutoffCount, before);
}

TEST(a_remote_rearm_takes_the_same_path_as_the_action_button) {
  Rig rig;
  cutOffLeaving(rig, 300);
  CHECK_EQ(rig.controller.state(), State_Cutoff);

  rig.controller.onRemote(Remote_Rearm);

  CHECK_EQ(rig.controller.state(), State_Idle);
  CHECK(rig.relay.isClosed());
}

TEST(a_remote_probe_skips_the_wait) {
  Rig rig;
  cutOffLeaving(rig, 300);

  // Without this the socket would sit blind for fifteen minutes before looking.
  rig.controller.onRemote(Remote_Probe);

  CHECK_EQ(rig.controller.state(), State_Probing);
  CHECK(rig.relay.isClosed());
}

TEST(a_remote_probe_is_ignored_unless_the_socket_is_cut) {
  Rig rig;
  rig.controller.begin();
  rig.startCharging(2000);

  // Probing is how a blind socket looks around. Mid-charge it can already see.
  rig.controller.onRemote(Remote_Probe);

  CHECK_EQ(rig.controller.state(), State_Charging);
}

TEST(no_remote_command_can_clear_a_stuck_relay_without_a_human) {
  Rig rig;
  cutOffLeaving(rig, 300);
  rig.stuckRelay = true;
  rig.run(2000, config::RelayStuckGraceMs + config::RelayStuckConfirmMs +
                    (4 * config::SampleIntervalMs));
  CHECK_EQ(rig.controller.state(), State_RelayStuck);

  // Cut and probe must not touch it. A welded contact needs someone to look at
  // the hardware, and a phone tapping "re-arm" from another room is not that.
  rig.controller.onRemote(Remote_Cut);
  CHECK_EQ(rig.controller.state(), State_RelayStuck);

  rig.controller.onRemote(Remote_Probe);
  CHECK_EQ(rig.controller.state(), State_RelayStuck);
}
