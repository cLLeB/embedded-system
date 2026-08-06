#if defined(ARDUINO)

#include "HalTelemetry.h"

#include "../core/Config.h"

namespace smartsocket {

HalTelemetry::HalTelemetry(unsigned long baud)
    : baud_(baud),
      lastPublishMs_(0),
      pending_(Remote_None),
      batteryPercent_(-1),
      batteryLimit_(-1),
      clientCharging_(-1),
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

    handleLine(rx_, len);
  }
}

void HalTelemetry::handleLine(const char* line, uint8_t len) {
  // Single letters are the original four, and stay exactly as they were.
  // A client that reconnects mid-line sends garbage, so anything unrecognised
  // is still dropped rather than faulted.
  if (len == 1) {
    switch (line[0]) {
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
    return;
  }

  // A<0|1> - hand the full-charge decision to the client, or take it back.
  if (len == 2 && (line[0] == 'A' || line[0] == 'a')) {
    if (line[1] == '1') {
      pending_ = Remote_AppManagedOn;
    } else if (line[1] == '0') {
      pending_ = Remote_AppManagedOff;
    }
    return;
  }

  // L<0|1> - the client can see itself charging, or cannot.
  //
  // The socket cannot work this out for a phone: 20 mA on 230 V is below one ADC
  // count, so its sensor reports a true zero and it concludes the outlet is
  // empty. Someone standing at it should not be told "waiting for something to
  // be plugged in" about a phone that is charging.
  if (len == 2 && (line[0] == 'L' || line[0] == 'l')) {
    if (line[1] == '1') {
      clientCharging_ = 1;
    } else if (line[1] == '0') {
      clientCharging_ = 0;
    }
    return;
  }

  // B<0..100> - the client's own battery level.
  // T<0..100> - the percentage it will cut at.
  //
  // Range-checked rather than clamped: a value outside 0-100 means the two ends
  // disagree about the format, and showing 100% because a corrupt line said 999
  // is exactly the sort of invented number the status parser refuses to produce.
  const char tag = line[0];
  const bool isBattery = (tag == 'B' || tag == 'b');
  const bool isLimit = (tag == 'T' || tag == 't');

  if ((isBattery || isLimit) && len >= 2 && len <= 4) {
    int16_t value = 0;
    for (uint8_t i = 1; i < len; i++) {
      if (line[i] < '0' || line[i] > '9') {
        return;
      }
      value = static_cast<int16_t>(value * 10 + (line[i] - '0'));
    }
    if (value >= 0 && value <= 100) {
      if (isBattery) {
        batteryPercent_ = value;
      } else {
        batteryLimit_ = value;
      }
    }
  }
}

int16_t HalTelemetry::takeBatteryPercent() {
  const int16_t value = batteryPercent_;
  batteryPercent_ = -1;
  return value;
}

int16_t HalTelemetry::takeBatteryLimit() {
  const int16_t value = batteryLimit_;
  batteryLimit_ = -1;
  return value;
}

int8_t HalTelemetry::takeClientCharging() {
  const int8_t value = clientCharging_;
  clientCharging_ = -1;
  return value;
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
