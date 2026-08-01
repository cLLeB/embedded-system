#if defined(ARDUINO)

#include "HalTelemetry.h"

#include "../core/Config.h"

namespace smartsocket {

HalTelemetry::HalTelemetry(unsigned long baud)
    : baud_(baud),
      lastPublishMs_(0),
      pending_(Remote_None),
      rxLen_(0),
      overflowed_(false) {
  rx_[0] = '\0';
}

void HalTelemetry::begin() { Serial.begin(baud_); }

void HalTelemetry::publish(const SocketStatus& status, Millis nowMs) {
  if ((nowMs - lastPublishMs_) < config::TelemetryIntervalMs) {
    return;
  }
  lastPublishMs_ = nowMs;

  // Comma-separated ordinals and integers, not JSON. A parser costs flash this
  // part does not have to spare, and the phone has the enum names anyway.
  //
  //   S,<state>,<mA>,<peakMa>,<thresholdMa>,<elapsedMs>,<cutoffs>,<savedMs>,<relay>
  Serial.print('S');
  Serial.print(',');
  Serial.print(static_cast<int>(status.state));
  Serial.print(',');
  Serial.print(status.currentMa);
  Serial.print(',');
  Serial.print(status.peakMa);
  Serial.print(',');
  Serial.print(status.thresholdMa);
  Serial.print(',');
  Serial.print(status.sessionElapsedMs);
  Serial.print(',');
  Serial.print(status.cutoffCount);
  Serial.print(',');
  Serial.print(status.totalSavedMs);
  Serial.print(',');
  Serial.print(status.relayClosed ? 1 : 0);
  Serial.print('\n');
}

void HalTelemetry::pump() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());

    if (c == '\r') {
      continue;
    }

    if (c != '\n') {
      if (rxLen_ < (sizeof(rx_) - 1)) {
        rx_[rxLen_++] = c;
      } else {
        // Mark it and keep draining. Silently truncating would turn a long line
        // into a valid-looking short command, which is worse than dropping it.
        overflowed_ = true;
      }
      continue;
    }

    rx_[rxLen_] = '\0';
    const uint8_t len = rxLen_;
    const bool bad = overflowed_;
    rxLen_ = 0;
    overflowed_ = false;

    if (bad || len == 0) {
      continue;
    }

    // Single letters only. A phone that reconnects mid-line sends garbage, so
    // anything unrecognised is dropped rather than faulted.
    if (len == 1) {
      switch (rx_[0]) {
        case 'C':
        case 'c':
          pending_ = Remote_Cut;
          break;
        case 'R':
        case 'r':
          pending_ = Remote_Rearm;
          break;
        case 'P':
        case 'p':
          pending_ = Remote_Probe;
          break;
        case '?':
          pending_ = Remote_StatusNow;
          break;
        default:
          break;
      }
    }
  }
}

RemoteCommand HalTelemetry::takeCommand() {
  pump();

  const RemoteCommand cmd = pending_;
  pending_ = Remote_None;

  if (cmd == Remote_StatusNow) {
    // Not a state change - just "talk to me". Forcing the next publish is the
    // whole effect, and the controller has nothing to do with it.
    lastPublishMs_ = 0;
    return Remote_None;
  }
  return cmd;
}

}  // namespace smartsocket

#endif  // ARDUINO
