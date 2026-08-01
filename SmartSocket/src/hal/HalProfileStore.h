// HalProfileStore.h - persists the learned profile in the Uno's EEPROM.
//
// All the interesting logic (framing, CRC, versioning) lives in core/ProfileCodec.
// This class is deliberately dull: read bytes, write bytes.
#ifndef SMARTSOCKET_HAL_HALPROFILESTORE_H
#define SMARTSOCKET_HAL_HALPROFILESTORE_H

#if defined(ARDUINO)

#include <Arduino.h>

#include "../core/Interfaces.h"
#include "../core/Types.h"

namespace smartsocket {

class HalProfileStore : public IProfileStore {
 public:
  explicit HalProfileStore(int baseAddress);

  bool load(DeviceProfile& out);
  void save(const DeviceProfile& profile);

 private:
  int baseAddress_;
};

}  // namespace smartsocket

#endif  // ARDUINO
#endif  // SMARTSOCKET_HAL_HALPROFILESTORE_H
