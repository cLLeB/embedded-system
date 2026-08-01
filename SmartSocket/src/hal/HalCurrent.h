// HalCurrent.h - ACS712 current sensing, in AC (true RMS) and DC flavours.
//
// Two engines behind one interface because AC and DC are not two settings of one
// algorithm. Mains current is a sine wave that averages to ~zero regardless of
// load, so averaging it would report an empty socket while a laptop charges. AC
// therefore needs true RMS over whole cycles; DC needs a simple mean.
#ifndef SMARTSOCKET_HAL_HALCURRENT_H
#define SMARTSOCKET_HAL_HALCURRENT_H

#if defined(ARDUINO)

#include <Arduino.h>

#include "../core/Interfaces.h"
#include "../core/Types.h"

namespace smartsocket {

// Shared zero-offset calibration. The ACS712 idles at Vcc/2, but the real value
// drifts with supply and part tolerance, so it is measured rather than assumed.
class HalCurrentSensorBase : public ICurrentSensor {
 public:
  explicit HalCurrentSensorBase(uint8_t pin);

  void calibrateZero();
  int32_t zeroAdc() const { return zeroAdc_; }

 protected:
  // The counts-to-milliamps arithmetic lives in core/CurrentMath.h, where it can
  // be tested without hardware.
  uint8_t pin_;
  int32_t zeroAdc_;
};

// True-RMS engine for mains. Samples across whole 50 Hz cycles.
class AcRmsCurrentSensor : public HalCurrentSensorBase {
 public:
  explicit AcRmsCurrentSensor(uint8_t pin) : HalCurrentSensorBase(pin) {}
  Milliamps read();
};

// Mean engine for low-voltage DC bench testing.
class DcCurrentSensor : public HalCurrentSensorBase {
 public:
  explicit DcCurrentSensor(uint8_t pin) : HalCurrentSensorBase(pin) {}
  Milliamps read();
};

}  // namespace smartsocket

#endif  // ARDUINO
#endif  // SMARTSOCKET_HAL_HALCURRENT_H
