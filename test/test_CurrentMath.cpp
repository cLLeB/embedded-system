// Tests for ADC-counts -> milliamps conversion and the integer square root.
//
// This math is what every other decision rests on: if the conversion is wrong,
// the taper threshold is wrong and the socket cuts power at the wrong moment.
#include "../SmartSocket/src/core/CurrentMath.h"

#include <cmath>

#include "../SmartSocket/src/core/Config.h"
#include "TestFramework.h"

using namespace smartsocket;
using namespace smartsocket::current_math;

TEST(isqrt_matches_the_real_square_root) {
  const uint32_t cases[] = {0, 1, 2, 3, 4, 8, 15, 16, 100, 255, 1024,
                            65535, 65536, 1000000, 262144, 0x3FFFFFFF};

  for (int i = 0; i < 16; ++i) {
    const uint32_t expected =
        static_cast<uint32_t>(std::sqrt(static_cast<double>(cases[i])));
    CHECK_EQ(isqrt32(cases[i]), expected);
  }
}

TEST(isqrt_truncates_rather_than_rounding) {
  // floor semantics: 15 -> 3, not 4.
  CHECK_EQ(isqrt32(15), 3);
  CHECK_EQ(isqrt32(24), 4);
  CHECK_EQ(isqrt32(99), 9);
}

TEST(isqrt_handles_the_largest_uint32) {
  // 65535^2 = 4294836225, just under 2^32. A shift bug here would wrap and give
  // a nonsense reading at high current.
  CHECK_EQ(isqrt32(4294836225u), 65535);
  CHECK_EQ(isqrt32(0xFFFFFFFFu), 65535);
}

TEST(isqrt_is_exact_on_perfect_squares) {
  for (uint32_t n = 0; n < 1000; ++n) {
    CHECK_EQ(isqrt32(n * n), n);
  }
}

TEST(adc_delta_converts_to_the_expected_current) {
  // The ACS712-5A gives 185 mV/A. One amp is therefore
  //   185 mV / (5000 mV / 1024 counts) = 37.9 counts.
  // 38 counts should read back as roughly 1 A.
  CHECK_NEAR(adcDeltaToMa(38), 1000, 40);

  // ~2 A
  CHECK_NEAR(adcDeltaToMa(76), 2000, 60);

  // ~4 A
  CHECK_NEAR(adcDeltaToMa(152), 4000, 80);
}

TEST(zero_counts_is_zero_current) {
  CHECK_EQ(adcDeltaToMa(0), 0);
}

TEST(sign_is_discarded_because_magnitude_is_what_matters) {
  // A reading below the zero point means direction, not a negative amount of
  // current. Both sides of zero must convert identically.
  CHECK_EQ(adcDeltaToMa(76), adcDeltaToMa(-76));
}

TEST(readings_inside_the_noise_floor_report_zero) {
  // ADC jitter must not be reported as a real load, or an empty socket looks
  // occupied.
  //
  // Derived from the constants rather than asserting on literal counts: the
  // clamp IS the subject here, so a hardcoded delta stops testing it the moment
  // the floor moves - and this floor has already moved twice, from the 3.7 V rig
  // to mains and again once the zero-offset bug was found.
  for (int32_t delta = 0; delta < 64; ++delta) {
    const int32_t mv = (delta * config::AdcRefMv) / config::AdcMaxCounts;
    const int32_t unclamped = (mv * 1000) / config::SensorMvPerAmp;

    if (unclamped < config::NoiseFloorMa) {
      CHECK_EQ(adcDeltaToMa(delta), 0);
    } else {
      CHECK_EQ(adcDeltaToMa(delta), unclamped);
    }
  }
}

TEST(the_noise_floor_does_not_swallow_a_real_load) {
  // The plug-detect threshold is 120 mA; the conversion has to report currents
  // at that level rather than clamping them away.
  CHECK(adcDeltaToMa(38) >= config::PlugDetectMa);
  const Milliamps atFloor = adcDeltaToMa(6);  // ~155 mA
  CHECK(atFloor > 0);
}

TEST(conversion_does_not_overflow_at_full_scale) {
  // Worst case: the ADC pinned at either rail. int32 must hold every
  // intermediate; a wrap here would report a huge current and trip a false fault.
  const Milliamps high = adcDeltaToMa(512);
  CHECK(high > 0);
  CHECK(high < 20000);

  const Milliamps pinned = adcDeltaToMa(1023);
  CHECK(pinned > 0);
  CHECK(pinned < 40000);
}

TEST(conversion_is_monotonic) {
  // More counts must never mean less current.
  Milliamps previous = 0;
  for (int32_t counts = 0; counts <= 512; ++counts) {
    const Milliamps ma = adcDeltaToMa(counts);
    CHECK(ma >= previous);
    previous = ma;
  }
}

TEST(rms_of_a_constant_signal_is_that_constant) {
  // A DC-like signal: every sample the same distance from zero. RMS must return
  // that distance, converted.
  const uint32_t count = 100;
  const uint32_t delta = 76;  // ~2 A
  const uint32_t sumSquares = count * delta * delta;

  CHECK_NEAR(rmsFromSumSquares(sumSquares, count), adcDeltaToMa(76), 30);
}

TEST(rms_of_a_sine_wave_is_peak_over_root_two) {
  // The real case: mains is a sine wave. Its RMS must come out at ~0.707 of the
  // peak, which is the entire reason AC cannot be measured by averaging.
  const uint32_t samples = 1000;
  const double peakCounts = 107.0;  // ~2 A RMS -> ~2.83 A peak

  uint32_t sumSquares = 0;
  for (uint32_t i = 0; i < samples; ++i) {
    const double phase = (2.0 * 3.14159265358979 * i) / samples;
    const int32_t v = static_cast<int32_t>(peakCounts * std::sin(phase));
    sumSquares += static_cast<uint32_t>(v * v);
  }

  const Milliamps rms = rmsFromSumSquares(sumSquares, samples);
  const Milliamps expected = adcDeltaToMa(static_cast<int32_t>(peakCounts / 1.41421356));
  CHECK_NEAR(rms, expected, 60);
}

TEST(rms_of_an_idle_sine_wave_is_zero) {
  // No load: only noise. Must read empty, not "something is charging".
  CHECK_EQ(rmsFromSumSquares(0, 500), 0);
}

TEST(rms_refuses_to_divide_by_a_zero_sample_count) {
  CHECK_EQ(rmsFromSumSquares(12345, 0), 0);
}

TEST(averaging_a_sine_wave_would_read_near_zero) {
  // This is the bug the AC engine exists to avoid, pinned down as a test: the
  // mean of a full sine cycle is ~0 regardless of amplitude, so a DC-style
  // average on mains would report an empty socket while a laptop charges.
  const uint32_t samples = 1000;
  const double peakCounts = 107.0;

  int32_t sum = 0;
  for (uint32_t i = 0; i < samples; ++i) {
    const double phase = (2.0 * 3.14159265358979 * i) / samples;
    sum += static_cast<int32_t>(peakCounts * std::sin(phase));
  }

  const Milliamps wrong = adcDeltaToMa(sum / static_cast<int32_t>(samples));
  CHECK_EQ(wrong, 0);

  // The RMS engine, on the same signal, sees the load.
  uint32_t sumSquares = 0;
  for (uint32_t i = 0; i < samples; ++i) {
    const double phase = (2.0 * 3.14159265358979 * i) / samples;
    const int32_t v = static_cast<int32_t>(peakCounts * std::sin(phase));
    sumSquares += static_cast<uint32_t>(v * v);
  }
  CHECK(rmsFromSumSquares(sumSquares, samples) > 1500);
}
