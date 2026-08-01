#include "CurrentMath.h"

#include "Config.h"

namespace smartsocket {
namespace current_math {

uint32_t isqrt32(uint32_t value) {
  uint32_t op = value;
  uint32_t res = 0;
  uint32_t one = 1UL << 30;  // highest power of four <= 2^32

  while (one > op) {
    one >>= 2;
  }
  while (one != 0) {
    if (op >= res + one) {
      op -= res + one;
      res += one << 1;
    }
    res >>= 1;
    one >>= 2;
  }
  return res;
}

Milliamps adcDeltaToMa(int32_t adcDelta) {
  if (adcDelta < 0) {
    adcDelta = -adcDelta;
  }

  // Two-step conversion keeps every intermediate inside int32 on an 8-bit part:
  //   counts -> millivolts -> milliamps
  // Doing it in one expression would overflow: 512 * 5000 * 1000 is ~2.6e9.
  const int32_t mv = (adcDelta * config::AdcRefMv) / config::AdcMaxCounts;
  const int32_t ma = (mv * 1000) / config::SensorMvPerAmp;

  if (ma < config::NoiseFloorMa) {
    return 0;
  }
  return ma;
}

Milliamps rmsFromSumSquares(uint32_t sumSquares, uint32_t count) {
  if (count == 0) {
    return 0;
  }
  const uint32_t meanSquare = sumSquares / count;
  return adcDeltaToMa(static_cast<int32_t>(isqrt32(meanSquare)));
}

}  // namespace current_math
}  // namespace smartsocket
