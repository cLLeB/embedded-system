// UiPresenter.h - turns a SocketStatus into two 16-character LCD lines.
//
// Pure formatting, no hardware: it renders into buffers the caller pushes to the
// display. That keeps "does the screen say the right thing" a native test rather
// than something you squint at on a desk.
//
// Portable C++: must not include Arduino.h.
#ifndef SMARTSOCKET_CORE_UIPRESENTER_H
#define SMARTSOCKET_CORE_UIPRESENTER_H

#include "Config.h"
#include "SocketController.h"
#include "Types.h"

namespace smartsocket {

// 16 visible columns plus a terminator.
const uint8_t LineBufferSize = config::LcdColumns + 1;

class UiPresenter {
 public:
  UiPresenter();

  void nextScreen();
  UiScreen screen() const { return screen_; }

  // Renders the current screen. Both buffers must be at least LineBufferSize.
  // Each line is always exactly LcdColumns chars plus a terminator, so the caller
  // never has to clear the display to erase stale characters.
  void render(const SocketStatus& status, char* line0, char* line1) const;

  // The client's own battery level, or -1 for "no client attached".
  //
  // Kept here rather than in SocketStatus on purpose: the state machine must
  // behave identically with and without a client, so a number only a client can
  // know has no business inside it. This is display, and display only.
  void setBatteryPercent(int16_t percent) { batteryPercent_ = percent; }
  int16_t batteryPercent() const { return batteryPercent_; }

  /** The percentage the client will cut at, or -1. */
  void setBatteryLimit(int16_t limit) { batteryLimit_ = limit; }

  /**
   * Whether the client can see itself charging.
   *
   * THE SOCKET CANNOT MEASURE THIS FOR A PHONE, so without being told it shows
   * "Ready / waiting for something to be plugged in" over an outlet that is
   * charging one. 20 mA on 230 V is below a single ADC count; the sensor is not
   * wrong, it is simply blind at that size. The client is the only thing that
   * knows, so when it says so, the display believes it.
   */
  void setClientCharging(bool charging) { clientCharging_ = charging; }

 private:
  void renderStatus(const SocketStatus& s, char* l0, char* l1) const;
  void renderDetail(const SocketStatus& s, char* l0, char* l1) const;
  void renderStats(const SocketStatus& s, char* l0, char* l1) const;

  UiScreen screen_;
  int16_t batteryPercent_;
  int16_t batteryLimit_;
  bool clientCharging_;
};

}  // namespace smartsocket

#endif  // SMARTSOCKET_CORE_UIPRESENTER_H
