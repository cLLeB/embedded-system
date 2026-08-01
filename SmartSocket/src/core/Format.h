// Format.h - small text helpers for the LCD.
//
// These exist instead of sprintf("%.2f") because AVR's float-enabled printf pulls
// in ~1.5 KB of flash on a 32 KB part. All formatting here is integer-only.
//
// Portable C++: must not include Arduino.h.
#ifndef SMARTSOCKET_CORE_FORMAT_H
#define SMARTSOCKET_CORE_FORMAT_H

#include <stdint.h>

#include "Types.h"

namespace smartsocket {
namespace format {

// Writes milliamps as amps with 2 decimals, e.g. 1240 -> "1.24". Negative inputs
// clamp to "0.00". Needs at least 8 bytes. Returns the number of chars written.
uint8_t amps(Milliamps ma, char* out, uint8_t outSize);

// Writes elapsed time as HH:MM:SS, e.g. 8133000 -> "02:15:33". Hours saturate at
// 99. Needs at least 9 bytes. Returns chars written.
uint8_t duration(Millis ms, char* out, uint8_t outSize);

// Writes elapsed time compactly as e.g. "12h30m". Needs at least 9 bytes: hours
// are not width-limited, so a large total reaches "1193h02m" plus a terminator.
uint8_t durationShort(Millis ms, char* out, uint8_t outSize);

// Writes an unsigned integer. Returns chars written.
uint8_t number(uint32_t value, char* out, uint8_t outSize);

// Human-readable state name, always <= 8 chars so it fits beside a current reading
// on a 16-column display.
const char* stateName(SocketState state);

// Copies src into a fixed-width field of exactly `width` chars, space-padded on the
// right and truncated if too long. Does NOT null-terminate beyond the field, so it
// can be used to build a line piecewise.
void padField(const char* src, char* out, uint8_t width);

}  // namespace format
}  // namespace smartsocket

#endif  // SMARTSOCKET_CORE_FORMAT_H
