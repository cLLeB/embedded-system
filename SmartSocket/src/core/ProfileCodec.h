// ProfileCodec.h - serialises the learned DeviceProfile to bytes, with integrity.
//
// The encode/decode/CRC logic lives in the core rather than the EEPROM HAL so it
// can be round-tripped and corruption-tested natively. The HAL is left with only
// raw byte access, which is the part that genuinely needs hardware.
//
// Portable C++: must not include Arduino.h.
#ifndef SMARTSOCKET_CORE_PROFILECODEC_H
#define SMARTSOCKET_CORE_PROFILECODEC_H

#include <stdint.h>

#include "Types.h"

namespace smartsocket {
namespace profile_codec {

// Wire format, little-endian:
//   [0..1] magic   [2] version   [3] reserved
//   [4..5] learnedTaperPct       [6..7] lastPeakMa
//   [8..9] cutoffCount           [10..13] totalSavedMs
//   [14..15] crc16 over bytes 0..13
const uint8_t RecordSize = 16;

const uint16_t Magic = 0x5A17;
const uint8_t Version = 1;

uint16_t crc16(const uint8_t* data, uint8_t length);

// Writes exactly RecordSize bytes into `out`.
void encode(const DeviceProfile& profile, uint8_t* out);

// Returns false and leaves `out` untouched if the magic, version or CRC do not
// check out - a blank or corrupted EEPROM must fall back to defaults rather than
// have the socket act on garbage thresholds.
bool decode(const uint8_t* in, DeviceProfile& out);

}  // namespace profile_codec
}  // namespace smartsocket

#endif  // SMARTSOCKET_CORE_PROFILECODEC_H
