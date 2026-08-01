// Config.h - every tunable constant for the smart socket, in one place.
// Portable C++: must not include Arduino.h.
//
// Tuning guide is in docs/TUNING.md. The values that matter most in the field are
// TaperConfirmMs and TaperRatioDefaultPct.
#ifndef SMARTSOCKET_CORE_CONFIG_H
#define SMARTSOCKET_CORE_CONFIG_H

#include <stdint.h>

namespace smartsocket {
namespace config {

// The pin map lives in src/hal/HalPins.h, not here: pin names like A0 are Arduino
// macros, and this header must stay free of Arduino.h so the core can be compiled
// and tested natively.

// ---------------------------------------------------------------------------
// ACS712 current sensor (5 A model)
// ---------------------------------------------------------------------------
// Datasheet sensitivity: 185 mV per amp for the 5 A variant.
const int32_t SensorMvPerAmp = 185;

// Uno ADC: 10-bit over a 5 V reference.
const int32_t AdcMaxCounts = 1024;
const int32_t AdcRefMv = 5000;

// How long the zero-offset calibration averages for.
//
// TIME-BASED, AND A WHOLE NUMBER OF MAINS CYCLES, FOR TWO REASONS.
//
// 1. Any AC load current present cancels instead of biasing the offset, because
//    a sine wave averages to zero over whole cycles. That is what makes it safe
//    to calibrate with the relay CLOSED - which it must be, because closed is
//    the condition the socket actually operates in.
// 2. A fixed sample count spans a whole number of cycles only by luck, and the
//    leftover part-cycle biases the result by an amount that wanders with phase.
//
// 100 ms = 5 cycles at 50 Hz.
const uint16_t ZeroCalibrationMs = 100;

// Samples thrown away before averaging starts. The relay has just closed, and
// its coil takes a few milliseconds to settle into a steady draw - which is the
// whole point of calibrating with it closed.
const uint16_t ZeroCalibrationDiscard = 64;

// How far the continuously tracked zero may sit from mid-scale before it is
// rejected. The ACS712 idles at VCC/2 by construction, so 100 counts - about
// half a volt - is not drift, it is a disconnected sensor or a broken supply,
// and following it would be a runaway.
const int32_t ZeroTrackingLimitCounts = 100;

// AC RMS sampling window. Ghana mains is 50 Hz = 20 ms per cycle; 60 ms spans
// exactly 3 whole cycles, which avoids the partial-cycle bias that would make the
// reading wander with phase.
//
// Kept at 3 cycles rather than 5 because this sampling loop blocks: every
// millisecond here is a millisecond loop() cannot poll the buttons, so a longer
// window starts swallowing short taps. Three cycles is enough to average cleanly
// and leaves the UI responsive. MUST stay a whole multiple of 20 ms.
const uint16_t AcRmsWindowMs = 60;
const uint16_t MainsFrequencyHz = 50;

// Samples averaged per DC reading.
const uint16_t DcAverageSamples = 64;

// Readings below this are clamped to zero rather than pretending to precision we
// do not have.
//
// TEMPORARILY 0 - THIS IS A MEASUREMENT BUILD, NOT A RELEASE.
//
// 60 was inherited from the battery rig and assumed one reading = one ADC count
// = 26 mA. That is wrong for AC mode: AcRmsCurrentSensor sums squares over a
// 60 ms window, which is roughly 500 samples, so the RMS estimate resolves well
// below a single count. The true floor is whatever an empty socket actually
// reads, and nobody has measured it.
//
// Set to 0 so the clamp is out of the way, read an empty socket, then set this
// just above whatever it shows. Everything below 60 mA - which is every phone
// on 230 V - depends on getting this right.
const int32_t NoiseFloorMa = 0;

// ---------------------------------------------------------------------------
// Charge detection thresholds
// ---------------------------------------------------------------------------
// THE LOAD IS A STAIRCASE, AND EVERY THRESHOLD HERE SITS BETWEEN TWO STEPS.
//
// This socket's "device" is a bank of 11 identical resistors in parallel across
// one 18650, and a charge is made to taper by pulling them out one at a time.
// The resistors were measured at 110 ohm +/-1%, so each one draws
// 3.7 V / 110 = 33.6 mA and the current can only ever land on a multiple of it:
//
//   resistors in |  1    2    3    4    5    6    7    8    9   10   11
//   current (mA) | 34   67  101  134  168  202  235  269  302  336  370
//
// One ADC count is 5000 mV / 1024 / 185 mV-per-A = 26 mA - most of a whole
// step. A threshold placed ON a step is therefore a coin toss, and the offset
// drift measured on this hardware (a phantom 100-120 mA before the zero
// calibration was moved after the relay closes) is several steps wide. So each
// threshold is placed as near as possible to the middle of a gap. Check the
// table above before changing any of them.

// EVERY THRESHOLD BELOW IS SIZED FOR 230 V MAINS, WHERE THE CURRENTS ARE TINY.
//
// A 35 W laptop charger draws 0.15 A at 230 V. The old battery rig's full load
// was 0.37 A, so moving to mains shrank every current in this project by 2-3x
// while these constants stayed put - which made a normally-charging laptop fall
// under MinSessionPeakMa and get cut at 60 s by the trickle-rejection branch.
//
// One ADC count is 26 mA. Measured on this hardware:
//
//   nothing plugged in           0 mA   (clamped by NoiseFloorMa)
//   charger in socket, no laptop 0 mA
//   laptop charging            150 mA   (~6 counts)
//
// There is very little room between those numbers. Do not tighten any of them
// further without re-measuring.

// A load drawing at least this much means something is plugged in.
// Just above the noise floor, because on mains a device that is merely running
// rather than charging can sit only one or two counts above zero, and it still
// has to start a session - otherwise an already-full laptop is never noticed and
// trickles forever, which is the case this product exists to prevent.
const int32_t PlugDetectMa = 70;

// Below this the load is gone entirely (unplugged), as opposed to merely full.
//
// Deliberately below the noise floor, i.e. only a reading of exactly zero counts
// as an unplug. Was 80 mA, which sat 13 mA under the 3-resistor step - so the
// demo's own end state was one count of drift away from being read as "the user
// pulled the plug", which silently abandons the session and returns to idle.
const int32_t UnplugMa = 50;

// Taper threshold never drops below this absolute floor, however low the peak was.
// Protects against a tiny-peak session producing a threshold buried in noise.
//
// 100 mA on mains. With a 150 mA charging peak the peak-derived threshold would
// be 58 mA - under the noise floor, so unreachable. This floor is what actually
// governs the cutoff at these currents: the socket cuts when the load falls
// below 100 mA, which sits between "laptop charging" (~150) and "laptop full but
// still running" (~65).
const int32_t TaperFloorMa = 100;

// A genuine charging device pulls at least this much at its peak. A load that
// never gets above it is a trickle, not a charge - which is what a full device
// left plugged in looks like after the user re-arms the socket without
// unplugging. Without this, such a session would set its own peak to the trickle
// level, making the taper threshold unreachable and trickle-charging forever.
//
// 120 mA on mains, measured against a real laptop charger drawing 150 mA. At
// 250 a normally-charging laptop fell under this and was cut at 60 s as an
// "already full" device - a false cutoff that looks exactly like a real one from
// the outside, which is why this value cannot be left at the battery rig's.
//
// The band PlugDetectMa (70) .. this (120) is the trickle-rejection feature: a
// load inside it is present but never charging. Under two ADC counts wide, which
// is as much as this sensor allows at mains voltages.
const int32_t MinSessionPeakMa = 120;

// ACS712-5A saturates at 5 A. Fault before then, with margin.
const int32_t OvercurrentMa = 4500;

// Any reading beyond this is physically impossible and implies a wiring or sensor
// fault rather than a real load.
const int32_t ImplausibleMa = 6000;

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
// How often the current is sampled and the state machine stepped.
const uint32_t SampleIntervalMs = 250;

// Load seen -> wait this long before trusting readings. Chargers draw a large
// inrush spike and negotiate power for the first few seconds; sampling the peak
// during that window would poison the baseline.
const uint32_t SettleMs = 5000;

// A session must run at least this long before a cutoff is even considered. Stops
// an already-full device from being cut off instantly on plug-in before a real
// peak has been established.
const uint32_t MinChargeMs = 60000;  // 60 s

// Current must stay below the taper threshold CONTINUOUSLY for this long before
// power is cut. This is the single most important guard in the system: it is what
// separates a real full-charge taper from a momentary dip mid-charge. Long is safe
// (a late cutoff wastes a little energy); short is dangerous (an early cutoff means
// the user's laptop never charges).
const uint32_t TaperConfirmMs = 90000;  // 90 s

// Load must read as gone for this long before declaring an unplug, so a brief
// dropout does not lose the session.
const uint32_t UnplugConfirmMs = 3000;

// ---------------------------------------------------------------------------
// Relay verification
// ---------------------------------------------------------------------------
// setClosed(false) IS A COMMAND, NOT A MEASUREMENT.
//
// Contacts weld, drive transistors fail, wires fall off a screw. Without this
// check the socket goes on displaying "FULL - POWER CUT" over a live outlet,
// and that is the single most dangerous thing this product can do: it is the
// one claim a user would actually act on.
//
// The ACS712 sits downstream of the relay, so if the contacts do not open the
// load current still flows through it. The failure is detectable with no extra
// hardware - it just has to be looked for.

// Current that counts as "still flowing" while the contacts are commanded open.
// Same bar as plug detection: below it a reading is not trustworthy anyway.
const int32_t RelayStuckMa = PlugDetectMa;

// Contacts part in milliseconds, but the smoothed reading and the load's own
// input capacitors take longer to fall. Do not start looking until they have.
const uint32_t RelayStuckGraceMs = 3000;

// Then it must stay up this long. One noisy sample is not a welded contact.
const uint32_t RelayStuckConfirmMs = 2000;

// ---------------------------------------------------------------------------
// Automatic recovery from a cutoff
// ---------------------------------------------------------------------------
// THE SENSOR IS DOWNSTREAM OF THE RELAY, SO AN OPEN RELAY MAKES THE SOCKET BLIND.
//
// While cut it cannot tell an empty socket from a full laptop from a phone at
// 5%: no current can flow, so there is nothing to measure. Waiting for a button
// press is not caution, it is the only input a blind socket has left - and a
// socket that has to be told when to work is not a smart socket.
//
// So instead of waiting, it closes the contacts briefly and looks. If a real
// charging current appears, something has changed and it resumes. If not, it
// opens again and waits longer.
//
// This is not auto-rearming, which would restart the trickle charging this
// product exists to prevent. Auto-rearming leaves the relay closed; this closes
// it for seconds at a time and only stays closed on positive evidence.

// How long the contacts stay closed while looking.
const uint32_t ProbeDurationMs = 8000;

// Current that must appear during a probe to resume. Same bar as a real
// session's peak: below this a load is present but not charging, which is
// exactly the device that was just cut.
const int32_t ProbeResumeMa = MinSessionPeakMa;

// Something was still drawing when power was cut, so a device is probably still
// sitting there, still full. Probing often would just re-energize a full
// battery, so this waits.
const uint32_t ProbeChargedIntervalMs = 900000;  // 15 min

// Nothing was drawing at cutoff, so the socket is probably empty. Energizing an
// empty socket costs nothing and wears nothing, so look often enough that
// plugging something in feels immediate.
const uint32_t ProbeEmptyIntervalMs = 60000;  // 60 s

// Below this the load at cutoff time counts as "nothing was there", selecting
// the fast probe interval. Same value as the unplug threshold, because it is the
// same question asked at a different moment.
const int32_t ProbeEmptyBelowMa = 60;

// ---------------------------------------------------------------------------
// Adaptive learning
// ---------------------------------------------------------------------------
// Starting taper threshold as a percentage of session peak, used before anything
// has been learned.
//
// 30% is the textbook CC/CV taper point and is what a mains charger wants. 38 is
// used instead because of the staircase above: 38% lifted by the 130% margin
// gives an effective 49%, which puts the threshold at 181 mA - almost exactly
// halfway between the 5-resistor step (168) and the 6-resistor step (202), the
// furthest it can possibly sit from both. The old 30% gave an effective 39%, or
// 144 mA, which is 10 mA off the 4-resistor step - well inside one ADC count.
//
// That staircase belonged to the low-voltage resistor rig. The mains build's
// load is a real charger, which tapers continuously rather than in 34 mA steps,
// so the textbook 30% applies directly and there are no gaps to aim between.
//
// This is a default, not a fixed point: the learning moves it toward whatever the
// attached load actually does. Put it back to 38 if the resistor bank is ever
// rebuilt.
const uint16_t TaperRatioDefaultPct = 30;

// The cutoff threshold sits this far ABOVE the learned taper ratio (130% = 30%
// higher). This margin is what stops the learning from eating itself.
//
// The learned ratio comes from the current at which a cutoff happened, which is
// by definition below the threshold that triggered it. Feed that straight back in
// and the threshold ratchets downward every session until it converges onto the
// device's trickle current - at which point "current < threshold" can never be
// true again and the socket silently stops cutting off forever.
//
// Holding the threshold a fixed margin above the learned value gives the loop a
// stable fixed point instead of an absorbing one: the learned ratio settles on
// the device's real taper, and the threshold settles just above it, still
// reachable. Do not remove this without re-reading
// test_ChargeAnalyzer.cpp: learning_converges_and_keeps_working_over_many_sessions.
const uint16_t TaperMarginPct = 130;

// Learned ratio is clamped to this band. Outside it, the reading is more likely a
// measurement artefact than a real device characteristic.
const uint16_t TaperRatioMinPct = 10;
const uint16_t TaperRatioMaxPct = 60;

// EMA weight (percent) given to the newest session when updating the learned ratio.
// 25% converges in a handful of sessions without letting one outlier dominate.
const uint16_t LearningWeightPct = 25;

// EMA smoothing for the current signal itself, as a percentage weight on the new
// sample. Lower = smoother but slower to react.
const int32_t CurrentSmoothingPct = 20;

// ---------------------------------------------------------------------------
// User interface
// ---------------------------------------------------------------------------
const uint8_t LcdColumns = 16;
const uint8_t LcdRows = 2;
const uint8_t LcdI2cAddress = 0x27;  // common backpack default; 0x3F also seen

const uint32_t ButtonDebounceMs = 30;
const uint32_t ButtonLongPressMs = 1500;

const uint32_t LcdRefreshMs = 500;

// Length of the power-on chirp. Long enough to be unmistakable, short enough not
// to be irritating on every boot.
const uint32_t BootChirpMs = 120;

}  // namespace config
}  // namespace smartsocket

#endif  // SMARTSOCKET_CORE_CONFIG_H
