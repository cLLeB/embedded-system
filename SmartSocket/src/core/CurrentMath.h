// CurrentMath.h - the arithmetic that turns ADC counts into milliamps.
//
// This lives in the core, not the HAL, because it is pure integer math with no
// hardware in it - and it is the part most likely to be subtly wrong. Keeping it
// here means the conversion and the RMS root are covered by native tests instead
// of being debugged with a multimeter.
//
// Portable C++: must not include Arduino.h.
#ifndef SMARTSOCKET_CORE_CURRENTMATH_H
#define SMARTSOCKET_CORE_CURRENTMATH_H

#include <stdint.h>

#include "Types.h"

namespace smartsocket {
namespace current_math {

// Integer square root (binary restoring method). Used instead of sqrt() from
// math.h because pulling the AVR float library onto a 32 KB part to compute one
// root per sample window is a poor trade.
uint32_t isqrt32(uint32_t value);

// Converts a distance from the sensor's zero point, in ADC counts, into
// milliamps. Sign is discarded: current magnitude is what matters, and a reading
// below zero is a direction, not a negative amount.
//
// Readings inside the ADC noise floor return 0 rather than implying precision the
// hardware does not have (~26 mA per LSB with a 10-bit ADC and 185 mV/A).
Milliamps adcDeltaToMa(int32_t adcDelta);

// Computes RMS milliamps from accumulated squared ADC deltas.
// Returns 0 if count is 0 rather than dividing by it.
Milliamps rmsFromSumSquares(uint32_t sumSquares, uint32_t count);

}  // namespace current_math
}  // namespace smartsocket

#endif  // SMARTSOCKET_CORE_CURRENTMATH_H
