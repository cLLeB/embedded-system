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

// Defined in SocketController.h. Forward-declared rather than included because
// that header includes this one, and ITelemetry only ever takes it by reference.
struct SocketStatus;

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

// Commands a remote client can send. Deliberately an enum rather than a string:
// the parsing belongs in the HAL, and the core should not learn that a wire
// protocol exists any more than it has learned that a relay has a coil.
enum RemoteCommand {
  Remote_None = 0,
  Remote_Cut,        // open the relay now
  Remote_Rearm,      // clear a cutoff and arm
  Remote_Probe,      // stop waiting, look for a load now
  Remote_StatusNow,  // send one status line immediately

  // Hand the full-charge decision to the client, and take it back.
  //
  // WHY THIS EXISTS. The ACS712-5A resolves 26 mA per count, and a charging
  // laptop draws about 150 mA against a MinSessionPeakMa of 120 - under two
  // counts of margin. Inside that margin the taper rule cannot reliably tell a
  // charging laptop from a full one, and when it gets it wrong it cuts a laptop
  // at 17%, which is far worse than not cutting at all.
  //
  // A connected client knows the actual battery percentage, which is the thing
  // the sensor was only ever a proxy for. So when one is attached and says so,
  // the socket stops guessing and does as it is told.
  //
  // Overcurrent, implausible readings and the stuck-relay check are NOT
  // suspended by this. Those are safety, not charge policy, and no client gets
  // to switch them off.
  Remote_AppManagedOn,
  Remote_AppManagedOff
};

// A link to something off-board - a phone, a PC, a serial monitor.
//
// Optional by construction: the socket is a complete product with no link
// attached, so nothing in the state machine may depend on one being present.
class ITelemetry {
 public:
  virtual ~ITelemetry() {}

  // Offered every sample tick. Implementations decide their own rate.
  virtual void publish(const SocketStatus& status, Millis nowMs) = 0;

  // One pending command, or Remote_None. Consumed by the call.
  virtual RemoteCommand takeCommand() = 0;

  // The client's own battery level, 0-100, or -1 if none has arrived since the
  // last call. Display only - nothing in the state machine reads it, because a
  // socket with no client attached must behave identically.
  //
  // Not pure: a link that cannot report one is still a valid link, and the
  // fakes in the test rig should not have to care.
  virtual int16_t takeBatteryPercent() { return -1; }

  // The percentage the client will cut at, 0-100, or -1 for none. Display only.
  virtual int16_t takeBatteryLimit() { return -1; }

  // Whether the client can see itself taking a charge.
  //
  // THE SOCKET CANNOT DETERMINE THIS FOR A PHONE. Charging on 230 V draws about
  // 20 mA, below a single ADC count on the ACS712-5A and below NoiseFloorMa, so
  // the sensor returns a true zero and the state machine concludes the outlet is
  // empty - correctly, from what it can measure. The client is the only thing
  // that knows otherwise, and someone standing at the socket should not be told
  // "waiting for something to be plugged in" about a phone that is charging.
  //
  // Display only, like the rest of this: 1 charging, 0 not, -1 nothing new.
  virtual int8_t takeClientCharging() { return -1; }
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
