// HalTelemetry.h - the socket's link to a phone, over the hardware UART.
//
// Whatever is on the other end - an HC-05, a USB serial monitor, a PC - this
// class does not care. It writes lines and reads lines.
//
// WHY THE HARDWARE UART AND NOT SoftwareSerial: SoftwareSerial disables
// interrupts for the whole duration of every byte it sends. This firmware has a
// 60 ms blocking RMS sampling window and drives the buzzer from Timer2, and
// millis() would lose time under it. The cost is that pins 0/1 are shared with
// the USB uploader, so an attached radio has to be unplugged to reflash.
#ifndef SMARTSOCKET_HAL_HALTELEMETRY_H
#define SMARTSOCKET_HAL_HALTELEMETRY_H

#if defined(ARDUINO)

#include <Arduino.h>

#include "../core/Interfaces.h"
#include "../core/SocketController.h"

namespace smartsocket {

class HalTelemetry : public ITelemetry {
 public:
  explicit HalTelemetry(unsigned long baud);

  void begin();

  void publish(const SocketStatus& status, Millis nowMs);
  RemoteCommand takeCommand();
  int16_t takeBatteryPercent();
  int16_t takeBatteryLimit();
  int8_t takeClientCharging();

 private:
  // Reads whatever bytes have arrived and parses complete lines. Never blocks:
  // a socket that stalls waiting for a phone to finish a sentence is a socket
  // that has stopped watching the load.
  void pump();

  // Decodes one complete line. Split out from pump() because the accepted
  // forms are no longer just single letters and the rules deserve one place.
  void handleLine(const char* line, uint8_t len);

  unsigned long baud_;
  Millis lastPublishMs_;
  RemoteCommand pending_;

  // Latest values the client reported, or -1 if none is unread.
  int16_t batteryPercent_;
  int16_t batteryLimit_;
  int8_t clientCharging_;

  // One line of input. Anything longer is a client fault, not a command, and is
  // discarded rather than overflowing.
  char rx_[16];
  uint8_t rxLen_;
  bool overflowed_;
};

}  // namespace smartsocket

#endif  // ARDUINO
#endif  // SMARTSOCKET_HAL_HALTELEMETRY_H
