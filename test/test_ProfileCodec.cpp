// Tests for EEPROM record framing, versioning and CRC.
#include "../SmartSocket/src/core/ProfileCodec.h"

#include "TestFramework.h"

using namespace smartsocket;
using namespace smartsocket::profile_codec;

namespace {

DeviceProfile sample() {
  DeviceProfile p;
  p.learnedTaperPct = 27;
  p.lastPeakMa = 1850;
  p.cutoffCount = 42;
  p.totalSavedMs = 987654321u;
  return p;
}

}  // namespace

TEST(a_profile_survives_a_round_trip) {
  uint8_t bytes[RecordSize];
  encode(sample(), bytes);

  DeviceProfile out;
  CHECK(decode(bytes, out));
  CHECK_EQ(out.learnedTaperPct, 27);
  CHECK_EQ(out.lastPeakMa, 1850);
  CHECK_EQ(out.cutoffCount, 42);
  CHECK_EQ(out.totalSavedMs, 987654321u);
}

TEST(a_blank_eeprom_is_rejected) {
  // Factory-fresh AVR EEPROM reads 0xFF everywhere. That must look like "nothing
  // stored", not like a profile.
  uint8_t bytes[RecordSize];
  for (uint8_t i = 0; i < RecordSize; ++i) {
    bytes[i] = 0xFF;
  }

  DeviceProfile out;
  CHECK_FALSE(decode(bytes, out));
}

TEST(a_zeroed_eeprom_is_rejected) {
  uint8_t bytes[RecordSize];
  for (uint8_t i = 0; i < RecordSize; ++i) {
    bytes[i] = 0x00;
  }

  DeviceProfile out;
  CHECK_FALSE(decode(bytes, out));
}

TEST(any_single_flipped_byte_is_caught) {
  // Acting on a corrupted threshold could mean cutting a laptop's power at the
  // wrong moment, so every byte must be covered by the CRC.
  for (uint8_t i = 0; i < RecordSize; ++i) {
    uint8_t bytes[RecordSize];
    encode(sample(), bytes);
    bytes[i] ^= 0xFF;

    DeviceProfile out;
    CHECK_FALSE(decode(bytes, out));
  }
}

TEST(a_single_flipped_bit_is_caught) {
  for (uint8_t i = 0; i < RecordSize; ++i) {
    for (uint8_t bit = 0; bit < 8; ++bit) {
      uint8_t bytes[RecordSize];
      encode(sample(), bytes);
      bytes[i] ^= static_cast<uint8_t>(1u << bit);

      DeviceProfile out;
      CHECK_FALSE(decode(bytes, out));
    }
  }
}

TEST(a_record_from_another_firmware_version_is_rejected) {
  uint8_t bytes[RecordSize];
  encode(sample(), bytes);
  bytes[2] = Version + 1;
  // Fix the CRC so only the version is wrong: a future format must be refused
  // rather than misread as this one.
  const uint16_t crc = crc16(bytes, 14);
  bytes[14] = static_cast<uint8_t>(crc & 0xFF);
  bytes[15] = static_cast<uint8_t>(crc >> 8);

  DeviceProfile out;
  CHECK_FALSE(decode(bytes, out));
}

TEST(decode_leaves_the_output_untouched_when_it_fails) {
  uint8_t bytes[RecordSize];
  encode(sample(), bytes);
  bytes[7] ^= 0x01;

  DeviceProfile out;
  out.learnedTaperPct = 31;
  out.lastPeakMa = 1;
  out.cutoffCount = 2;
  out.totalSavedMs = 3;

  CHECK_FALSE(decode(bytes, out));
  // The caller's defaults must survive so it can fall back cleanly.
  CHECK_EQ(out.learnedTaperPct, 31);
  CHECK_EQ(out.cutoffCount, 2);
}

TEST(crc_matches_the_ccitt_false_reference_vector) {
  // "123456789" -> 0x29B1 is the standard check value for CRC-16/CCITT-FALSE.
  const uint8_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  CHECK_EQ(crc16(data, 9), 0x29B1);
}

TEST(extreme_values_round_trip) {
  DeviceProfile p;
  p.learnedTaperPct = 0xFFFF;
  p.lastPeakMa = 0xFFFF;
  p.cutoffCount = 0xFFFF;
  p.totalSavedMs = 0xFFFFFFFFu;

  uint8_t bytes[RecordSize];
  encode(p, bytes);

  DeviceProfile out;
  CHECK(decode(bytes, out));
  CHECK_EQ(out.learnedTaperPct, 0xFFFF);
  CHECK_EQ(out.totalSavedMs, 0xFFFFFFFFu);
}
