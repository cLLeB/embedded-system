// Types.h - shared value types and enums for the smart socket core.
// Portable C++: must not include Arduino.h.
#ifndef SMARTSOCKET_CORE_TYPES_H
#define SMARTSOCKET_CORE_TYPES_H

#include <stdint.h>

namespace smartsocket {

// Milliseconds since boot. Matches Arduino millis() width so it wraps identically
// on hardware and in tests.
typedef uint32_t Millis;

// Current in milliamps. Signed so a sensor below its zero point reads negative
// instead of wrapping to a huge positive value.
typedef int32_t Milliamps;

enum SocketState {
  State_Calibrating = 0,  // measuring sensor zero offset, relay open
  State_Idle,             // relay closed, waiting for a device to draw current
  State_Settling,         // load seen; ignoring inrush before trusting readings
  State_Charging,         // tracking the charge curve
  State_Cutoff,           // full charge detected, relay open
  State_Fault,            // overcurrent or implausible reading, relay open
  State_ManualOff,        // user forced power off
  State_Probing,          // relay briefly closed from Cutoff, looking for a load
  State_RelayStuck        // commanded open, current still flowing: contacts failed
};

enum ButtonId {
  Button_Next = 0,
  Button_Action,
  Button_Count
};

enum ButtonEvent {
  ButtonEvent_None = 0,
  ButtonEvent_ShortPress,
  ButtonEvent_LongPress
};

enum BuzzerPattern {
  Buzz_Silent = 0,
  Buzz_PlugDetected,  // one short chirp
  Buzz_Cutoff,        // three beeps, twice
  Buzz_Fault          // slow continuous beep until cleared
};

enum UiScreen {
  Screen_Status = 0,
  Screen_Detail,
  Screen_Stats,
  Screen_Count
};

// Why the analyzer thinks the session ended. Kept distinct from SocketState so the
// analyzer stays a pure decision function with no knowledge of relays or UI.
enum ChargeVerdict {
  Verdict_Continue = 0,  // still charging, do nothing
  Verdict_Full,          // taper confirmed -> cut power
  Verdict_Unplugged,     // load gone -> re-arm
  Verdict_Overcurrent    // beyond safe range -> fault
};

// Learned, persisted device profile. Survives power cycles in EEPROM.
struct DeviceProfile {
  uint16_t learnedTaperPct;  // taper threshold as a percentage of session peak
  uint16_t lastPeakMa;       // peak current of the most recent completed session
  uint16_t cutoffCount;      // lifetime successful cutoffs
  uint32_t totalSavedMs;     // cumulative time power was held off after full charge
};

}  // namespace smartsocket

#endif  // SMARTSOCKET_CORE_TYPES_H
