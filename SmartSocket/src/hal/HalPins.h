// HalPins.h - the physical pin map.
//
// Separate from core/Config.h because pin names like A0 are Arduino macros, and
// the core must stay compilable on a PC where they do not exist.
#ifndef SMARTSOCKET_HAL_HALPINS_H
#define SMARTSOCKET_HAL_HALPINS_H

#if defined(ARDUINO)

#include <Arduino.h>

namespace smartsocket {
namespace pins {

// ACS712 analog output.
const uint8_t CurrentSensor = A0;

// A4 (SDA) and A5 (SCL) are the Uno's hardware I2C pins. The LCD backpack uses
// them via the Wire library, so they are reserved and must not be reassigned.

// Buttons wire to GND and use INPUT_PULLUP, so no external resistors are needed.
const uint8_t ButtonNext = 2;
const uint8_t ButtonAction = 3;

const uint8_t Relay = 7;
const uint8_t Buzzer = 8;

// Most cheap 1-channel relay boards are active-LOW: driving the pin LOW energizes
// the coil and closes the contact. Set to false if yours is active-HIGH (test with
// the relay disconnected from any load first).
const bool RelayActiveLow = true;

}  // namespace pins
}  // namespace smartsocket

#endif  // ARDUINO
#endif  // SMARTSOCKET_HAL_HALPINS_H
