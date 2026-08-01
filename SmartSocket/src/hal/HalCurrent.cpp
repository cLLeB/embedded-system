#if defined(ARDUINO)

#include "HalCurrent.h"

#include "../core/Config.h"
#include "../core/CurrentMath.h"

namespace smartsocket {

HalCurrentSensorBase::HalCurrentSensorBase(uint8_t pin)
    : pin_(pin), zeroAdc_(config::AdcMaxCounts / 2) {}

void HalCurrentSensorBase::calibrateZero() {
  // THIS RUNS WITH THE RELAY CLOSED, ON PURPOSE.
  //
  // The ACS712 is ratiometric to its own VCC, and it shares the 5 V rail with the
  // relay coil. Energizing the coil pulls ~70 mA through the jumper wires and
  // breadboard contacts feeding the sensor, dropping its supply by a few tens of
  // millivolts against the ADC's reference and shifting its midpoint.
  //
  // Calibrating with the relay OPEN measured a zero that never applies in
  // service: the socket spends its whole working life with the contacts closed.
  // On this hardware that error was 80-100 mA of phantom current on an empty
  // socket - three to four ADC counts, larger than anything the sensor is being
  // asked to resolve, and it drifted with rail loading and temperature.
  //
  // Averaging over whole mains cycles is what makes closed-relay calibration
  // safe: a load that happens to be plugged in at boot draws a sine wave, which
  // averages to zero, so it cancels rather than being calibrated away.

  for (uint16_t i = 0; i < config::ZeroCalibrationDiscard; ++i) {
    (void)analogRead(pin_);
  }

  uint32_t sum = 0;
  uint32_t count = 0;
  const uint32_t start = millis();
  while ((millis() - start) < config::ZeroCalibrationMs) {
    sum += static_cast<uint32_t>(analogRead(pin_));
    ++count;
  }

  if (count > 0) {
    zeroAdc_ = static_cast<int32_t>(sum / count);
  }
}

Milliamps AcRmsCurrentSensor::read() {
  uint32_t sumSquares = 0;
  uint32_t sum = 0;
  uint32_t count = 0;

  // Sampling for a whole number of 50 Hz cycles avoids the partial-cycle bias
  // that would make the reading wander with phase.
  const uint32_t start = millis();
  while ((millis() - start) < config::AcRmsWindowMs) {
    const int32_t raw = static_cast<int32_t>(analogRead(pin_));
    sum += static_cast<uint32_t>(raw);

    const int32_t delta = raw - zeroAdc_;
    // Worst case 1023^2 * ~890 samples ~= 9.3e8, comfortably inside uint32.
    sumSquares += static_cast<uint32_t>(delta * delta);
    ++count;
  }

  const Milliamps result = current_math::rmsFromSumSquares(sumSquares, count);

  // THE ZERO IS TRACKED, NOT FROZEN AT BOOT.
  //
  // A one-shot calibration goes stale: the ACS712's offset drifts with
  // temperature and with 5 V rail loading, and on this hardware it walked far
  // enough within a couple of minutes to fake a 0.10 A load on an empty socket -
  // which the socket then dutifully "charged" and cut off.
  //
  // Over a whole number of mains cycles a real AC load current averages to zero,
  // so the mean of the raw samples IS the offset, load or no load. Feeding it
  // forward means the next window is measured against an offset that is
  // milliseconds old instead of hours old.
  //
  // Used for the NEXT window rather than this one on purpose: re-deriving the
  // mean and the deviation from the same samples needs either 64-bit arithmetic
  // or a second pass, and the offset moves over minutes - a 60 ms lag is
  // irrelevant against that.
  if (count > 0) {
    const int32_t mean = static_cast<int32_t>(sum / count);

    // A tracked value with no bound is a runaway waiting to happen. The sensor
    // idles at mid-scale by construction; this far out is a fault, not drift.
    const int32_t mid = config::AdcMaxCounts / 2;
    if (mean > mid - config::ZeroTrackingLimitCounts &&
        mean < mid + config::ZeroTrackingLimitCounts) {
      zeroAdc_ = mean;
    }
  }

  return result;
}

Milliamps DcCurrentSensor::read() {
  int32_t sum = 0;
  for (uint16_t i = 0; i < config::DcAverageSamples; ++i) {
    sum += static_cast<int32_t>(analogRead(pin_)) - zeroAdc_;
  }
  return current_math::adcDeltaToMa(sum /
                                    static_cast<int32_t>(config::DcAverageSamples));
}

}  // namespace smartsocket

#endif  // ARDUINO
