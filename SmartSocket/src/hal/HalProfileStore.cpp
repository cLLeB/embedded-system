#if defined(ARDUINO)

#include "HalProfileStore.h"

#include <EEPROM.h>

#include "../core/ProfileCodec.h"

namespace smartsocket {

HalProfileStore::HalProfileStore(int baseAddress) : baseAddress_(baseAddress) {}

bool HalProfileStore::load(DeviceProfile& out) {
  uint8_t buffer[profile_codec::RecordSize];
  for (uint8_t i = 0; i < profile_codec::RecordSize; ++i) {
    buffer[i] = EEPROM.read(baseAddress_ + i);
  }
  return profile_codec::decode(buffer, out);
}

void HalProfileStore::save(const DeviceProfile& profile) {
  uint8_t buffer[profile_codec::RecordSize];
  profile_codec::encode(profile, buffer);

  for (uint8_t i = 0; i < profile_codec::RecordSize; ++i) {
    // EEPROM.update, not write: it skips the cell if the byte is unchanged. AVR
    // EEPROM is rated for ~100k erase cycles, and most saves change only a couple
    // of bytes, so this materially extends its life.
    EEPROM.update(baseAddress_ + i, buffer[i]);
  }
}

}  // namespace smartsocket

#endif  // ARDUINO
