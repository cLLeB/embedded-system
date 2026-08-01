// HalDisplay.h - 16x2 character LCD on an I2C (PCF8574) backpack.
//
// Wraps the LiquidCrystal_I2C library. Keeping it behind IDisplay means swapping
// the LCD, or moving to a different library, touches only this file.
#ifndef SMARTSOCKET_HAL_HALDISPLAY_H
#define SMARTSOCKET_HAL_HALDISPLAY_H

#if defined(ARDUINO)

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>

#include "../core/Interfaces.h"

namespace smartsocket {

// The only two addresses these backpacks are ever found at, because the address
// jumpers are left open: a PCF8574 lands on 0x27, a PCF8574A on 0x3F. The chips
// are pin-compatible and the boards are not marked, so which one is fitted
// cannot be told by looking - only by asking the bus.
const uint8_t LcdAddressPcf8574 = 0x27;
const uint8_t LcdAddressPcf8574A = 0x3F;

class HalDisplay : public IDisplay {
 public:
  HalDisplay(uint8_t address, uint8_t columns, uint8_t rows);

  // Probes the configured address, and falls back to the other one if only that
  // answers. A wrong address is silent - no error, backlight still on, just a
  // blank panel - so it is worth two bus transactions at boot to rule out.
  void begin();

  void showLine(uint8_t row, const char* text);

  // Which address actually answered. Only meaningful after begin().
  uint8_t address() const { return address_; }

 private:
  // Two instances rather than one, because LiquidCrystal_I2C takes its address
  // at construction and offers no way to change it afterwards. Eleven bytes of
  // RAM is a cheaper price than dynamic allocation on a 2 KB part.
  LiquidCrystal_I2C lcdPrimary_;
  LiquidCrystal_I2C lcdAlternate_;
  LiquidCrystal_I2C* lcd_;
  uint8_t address_;
  uint8_t alternateAddress_;
  uint8_t columns_;
  uint8_t rows_;
};

}  // namespace smartsocket

#endif  // ARDUINO
#endif  // SMARTSOCKET_HAL_HALDISPLAY_H
