#include "ProfileCodec.h"

namespace smartsocket {
namespace profile_codec {
namespace {

void put16(uint8_t* p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v & 0xFF);
  p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}

uint16_t get16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0]) |
         (static_cast<uint16_t>(p[1]) << 8);
}

void put32(uint8_t* p, uint32_t v) {
  p[0] = static_cast<uint8_t>(v & 0xFF);
  p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
  p[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
  p[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

uint32_t get32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) |
         (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

}  // namespace

uint16_t crc16(const uint8_t* data, uint8_t length) {
  // CRC-16/CCITT-FALSE. Bitwise rather than table-driven: a 512-byte lookup table
  // would cost more flash than the loop saves cycles, and this runs twice a
  // session.
  uint16_t crc = 0xFFFF;
  for (uint8_t i = 0; i < length; ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (uint8_t bit = 0; bit < 8; ++bit) {
      if (crc & 0x8000) {
        crc = static_cast<uint16_t>((crc << 1) ^ 0x1021);
      } else {
        crc = static_cast<uint16_t>(crc << 1);
      }
    }
  }
  return crc;
}

void encode(const DeviceProfile& profile, uint8_t* out) {
  if (out == 0) {
    return;
  }

  put16(out + 0, Magic);
  out[2] = Version;
  out[3] = 0;
  put16(out + 4, profile.learnedTaperPct);
  put16(out + 6, profile.lastPeakMa);
  put16(out + 8, profile.cutoffCount);
  put32(out + 10, profile.totalSavedMs);
  put16(out + 14, crc16(out, 14));
}

bool decode(const uint8_t* in, DeviceProfile& out) {
  if (in == 0) {
    return false;
  }
  if (get16(in + 0) != Magic) {
    return false;  // blank or never-written EEPROM
  }
  if (in[2] != Version) {
    return false;  // format from another firmware build; do not guess at it
  }
  if (get16(in + 14) != crc16(in, 14)) {
    return false;  // corrupted; defaults are safer than bad thresholds
  }

  out.learnedTaperPct = get16(in + 4);
  out.lastPeakMa = get16(in + 6);
  out.cutoffCount = get16(in + 8);
  out.totalSavedMs = get32(in + 10);
  return true;
}

}  // namespace profile_codec
}  // namespace smartsocket
