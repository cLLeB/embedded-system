#if defined(ARDUINO)

#include "HalDisplay.h"

#include <Wire.h>

namespace smartsocket {
namespace {

// A device that ACKs its address is there. Nothing else on this bus does.
bool addressAnswers(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

}  // namespace

HalDisplay::HalDisplay(uint8_t address, uint8_t columns, uint8_t rows)
    : lcdPrimary_(address, columns, rows),
      lcdAlternate_(address == LcdAddressPcf8574 ? LcdAddressPcf8574A
                                                 : LcdAddressPcf8574,
                    columns, rows),
      lcd_(&lcdPrimary_),
      address_(address),
      alternateAddress_(address == LcdAddressPcf8574 ? LcdAddressPcf8574A
                                                     : LcdAddressPcf8574),
      columns_(columns),
      rows_(rows) {}

void HalDisplay::begin() {
  Wire.begin();

  // WITHOUT THIS, A NOISE GLITCH ON THE I2C LEADS FREEZES THE WHOLE SOCKET.
  //
  // Wire blocks forever if a device holds SDA low, and this build gives it every
  // chance to happen: the relay switches 230 V a few centimetres from unshielded
  // flying leads, and contact arcing couples straight into SDA/SCL. A confused
  // backpack then holds the line and Wire never returns.
  //
  // The consequence is not a blank display. loop() stops running, so pin 7 holds
  // whatever level it last had - and if that was mid-charge or mid-probe, the
  // relay stays CLOSED. Power stuck on, with the display frozen on whatever it
  // last drew. On a mains socket whose whole purpose is cutting power, a lockup
  // that fails ON is the worst possible failure.
  //
  // A timeout turns that into a dropped frame instead: give up after 25 ms and
  // reset the bus, then carry on.
  Wire.setWireTimeout(25000, true);

  // Only switch on positive evidence: the configured address staying silent
  // while the other one answers. If neither answers, or both do, the configured
  // one stands - guessing past that would trade a diagnosable fault for a
  // confusing one.
  if (!addressAnswers(address_) && addressAnswers(alternateAddress_)) {
    lcd_ = &lcdAlternate_;
    address_ = alternateAddress_;
  }

  lcd_->init();
  lcd_->backlight();
  lcd_->clear();
}

void HalDisplay::showLine(uint8_t row, const char* text) {
  if (row >= rows_ || text == 0) {
    return;
  }

  lcd_->setCursor(0, row);

  // The presenter always hands over exactly `columns_` characters, so writing the
  // full width overwrites anything stale. That is why there is no clear() here:
  // clearing between frames is what makes these displays flicker.
  uint8_t written = 0;
  for (; written < columns_ && text[written] != '\0'; ++written) {
    lcd_->write(text[written]);
  }
  for (; written < columns_; ++written) {
    lcd_->write(' ');
  }
}

}  // namespace smartsocket

#endif  // ARDUINO
