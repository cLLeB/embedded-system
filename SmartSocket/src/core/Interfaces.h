// Interfaces.h - the seam between decision logic and hardware.
//
// Every interface here is implemented twice: once in src/hal/ against real
// hardware, and once in test/fakes/ against nothing at all. The core depends only
// on these declarations, which is what lets a three-hour charge session be tested
// in microseconds on a PC with no Arduino attached.
//
// Portable C++: must not include Arduino.h.
#ifndef SMARTSOCKET_CORE_INTERFACES_H
#define SMARTSOCKET_CORE_INTERFACES_H

#include "Types.h"

namespace smartsocket {

// Time source. Injected rather than calling millis() directly so tests can advance
// time instantly instead of waiting for it.
class IClock {
 public:
  virtual ~IClock() {}
  virtual Millis now() const = 0;
};

class ICurrentSensor {
 public:
  virtual ~ICurrentSensor() {}

  // Establishes the zero-current reference. The caller MUST guarantee the relay is
  // open (no current flowing) before calling this.
  virtual void calibrateZero() = 0;

  // Current magnitude through the sensor. For AC this is true RMS; for DC it is a
  // mean. Readings inside the noise floor are clamped to 0.
  virtual Milliamps read() = 0;
};

class IRelay {
 public:
  virtual ~IRelay() {}

  // closed == true means the load is energized.
  virtual void setClosed(bool closed) = 0;
  virtual bool isClosed() const = 0;
};

class IDisplay {
 public:
  virtual ~IDisplay() {}
  virtual void begin() = 0;

  // Writes one pre-formatted, exactly-16-character line. Row is 0 or 1.
  virtual void showLine(uint8_t row, const char* text) = 0;
};

class IBuzzer {
 public:
  virtual ~IBuzzer() {}
  virtual void setSounding(bool sounding) = 0;
};

// Raw button levels. Debouncing and press classification happen in the core
// (ButtonDebouncer), not here, so they can be tested without hardware.
class IButtonSource {
 public:
  virtual ~IButtonSource() {}

  // true == physically pressed. Implementations hide active-low pull-up wiring.
  virtual bool isPressed(ButtonId id) const = 0;
};

// Persistence for the learned device profile.
class IProfileStore {
 public:
  virtual ~IProfileStore() {}

  // Returns false if nothing valid was stored (first boot, or CRC mismatch), in
  // which case out is left untouched and the caller should use defaults.
  virtual bool load(DeviceProfile& out) = 0;
  virtual void save(const DeviceProfile& profile) = 0;
};

}  // namespace smartsocket

#endif  // SMARTSOCKET_CORE_INTERFACES_H
