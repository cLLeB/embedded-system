#include "UiPresenter.h"

#include "Format.h"

namespace smartsocket {
namespace {

const uint8_t Cols = config::LcdColumns;

// Writes `left` flush-left and `right` flush-right across exactly Cols columns,
// spaces between. If they would collide, `right` wins: the number is the
// information, the label is the decoration.
void composeLR(const char* left, const char* right, char* out) {
  for (uint8_t i = 0; i < Cols; ++i) {
    out[i] = ' ';
  }

  uint8_t rightLen = 0;
  if (right != 0) {
    // Bounds first, then dereference: the other order reads right[Cols] before
    // deciding it is out of range.
    while (rightLen < Cols && right[rightLen] != '\0') {
      ++rightLen;
    }
  }

  const uint8_t rightStart = static_cast<uint8_t>(Cols - rightLen);
  for (uint8_t i = 0; i < rightLen; ++i) {
    out[rightStart + i] = right[i];
  }

  if (left != 0) {
    for (uint8_t i = 0; i < rightStart && left[i] != '\0'; ++i) {
      out[i] = left[i];
    }
  }

  out[Cols] = '\0';
}

// Writes a single string across the full width, space-padded.
void composeFull(const char* text, char* out) {
  format::padField(text, out, Cols);
  out[Cols] = '\0';
}

// Appends src to dst at *pos, advancing it. Bounded by size.
void appendStr(char* dst, uint8_t& pos, uint8_t size, const char* src) {
  while (src != 0 && *src != '\0' && pos < static_cast<uint8_t>(size - 1)) {
    dst[pos++] = *src++;
  }
  dst[pos] = '\0';
}

}  // namespace

UiPresenter::UiPresenter() : screen_(Screen_Status), batteryPercent_(-1) {}

void UiPresenter::nextScreen() {
  screen_ = static_cast<UiScreen>((screen_ + 1) % Screen_Count);
}

void UiPresenter::renderStatus(const SocketStatus& s, char* l0, char* l1) const {
  char amps[10];
  char right[12];
  uint8_t pos = 0;

  // The top-right corner is the most valuable space on the display, so it
  // carries the best answer available to "how full is it". A real battery
  // percentage from the client beats the current every time - the current was
  // only ever a proxy for it, and a poor one at 26 mA of resolution.
  right[0] = '\0';

  if (batteryPercent_ >= 0) {
    char pct[8];
    format::number(static_cast<uint16_t>(batteryPercent_), pct, sizeof(pct));
    appendStr(right, pos, sizeof(right), pct);
    appendStr(right, pos, sizeof(right), "%");
  } else {
    format::amps(s.currentMa, amps, sizeof(amps));
    appendStr(right, pos, sizeof(right), amps);
    appendStr(right, pos, sizeof(right), "A");
  }

  composeLR(format::stateName(s.state), right, l0);

  char elapsed[10];
  format::duration(s.sessionElapsedMs, elapsed, sizeof(elapsed));

  // How long this charge has been running, and - only once the percentage has
  // taken the top line - the live current underneath it.
  //
  // The session peak used to sit here when there was no client. It is gone on
  // purpose: peak current is a number for diagnosing the algorithm, not for
  // someone glancing at a socket to see how their laptop is doing. Better an
  // empty half-line than a figure that means nothing to whoever is reading it.
  pos = 0;
  right[0] = '\0';

  if (batteryPercent_ >= 0) {
    format::amps(s.currentMa, amps, sizeof(amps));
    appendStr(right, pos, sizeof(right), amps);
    appendStr(right, pos, sizeof(right), "A");
  }

  composeLR(elapsed, right, l1);
}

void UiPresenter::renderDetail(const SocketStatus& s, char* l0, char* l1) const {
  char amps[10];
  char right[12];
  uint8_t pos = 0;

  format::amps(s.thresholdMa, amps, sizeof(amps));
  right[0] = '\0';
  appendStr(right, pos, sizeof(right), amps);
  appendStr(right, pos, sizeof(right), "A");
  composeLR("Cut at", right, l0);

  char pct[8];
  format::number(s.taperRatioPct, pct, sizeof(pct));
  char left[14];
  pos = 0;
  left[0] = '\0';
  appendStr(left, pos, sizeof(left), "Taper ");
  appendStr(left, pos, sizeof(left), pct);
  appendStr(left, pos, sizeof(left), "%");

  char count[8];
  format::number(s.cutoffCount, count, sizeof(count));
  pos = 0;
  right[0] = '\0';
  appendStr(right, pos, sizeof(right), "n=");
  appendStr(right, pos, sizeof(right), count);
  composeLR(left, right, l1);
}

void UiPresenter::renderStats(const SocketStatus& s, char* l0, char* l1) const {
  char count[8];
  format::number(s.cutoffCount, count, sizeof(count));
  composeLR("Cutoffs", count, l0);

  char saved[10];
  format::durationShort(s.totalSavedMs, saved, sizeof(saved));
  composeLR("Saved", saved, l1);
}

void UiPresenter::render(const SocketStatus& status, char* line0,
                         char* line1) const {
  // Cutoff and Fault are the states the user must act on, so they take the screen
  // regardless of which one was selected. Silently leaving a stats page up while
  // power is cut would be the display lying about what matters.
  if (status.state == State_Cutoff) {
    composeFull("FULL - POWER CUT", line0);
    // Not "Press ACTION" any more: the socket re-checks on its own, so a demand
    // for a button press would be the display lying about what is required.
    // ACTION still works - it just skips the wait.
    composeFull("ACTION=check now", line1);
    return;
  }
  if (status.state == State_Probing) {
    // Eight seconds of the relay clicking and the socket going live needs an
    // explanation on screen, or it reads as a fault.
    composeFull("CHECKING SOCKET", line0);

    char amps[10];
    char right[12];
    uint8_t pos = 0;
    format::amps(status.currentMa, amps, sizeof(amps));
    right[0] = '\0';
    appendStr(right, pos, sizeof(right), amps);
    appendStr(right, pos, sizeof(right), "A");
    composeLR("Load", right, line1);
    return;
  }
  if (status.state == State_RelayStuck) {
    // The one message that must never be softened. Every other screen describes
    // what the socket has done; this one says the socket is not in control and
    // the outlet is live regardless of what it says elsewhere.
    composeFull("! RELAY STUCK !", line0);
    composeFull("UNPLUG AT WALL", line1);
    return;
  }
  if (status.state == State_Fault) {
    composeFull("! OVERCURRENT !", line0);
    // A short tap is what clears a fault. Holding ACTION is the manual-off
    // toggle, which from here would drop the user into MANUAL POWER OFF instead
    // of re-arming - so the instruction has to say tap, not hold.
    composeFull("Tap ACTION", line1);
    return;
  }
  if (status.state == State_ManualOff) {
    composeFull("MANUAL POWER OFF", line0);
    composeFull("Hold ACTION=ON", line1);
    return;
  }

  switch (screen_) {
    case Screen_Detail:
      renderDetail(status, line0, line1);
      break;
    case Screen_Stats:
      renderStats(status, line0, line1);
      break;
    case Screen_Status:
    default:
      renderStatus(status, line0, line1);
      break;
  }
}

}  // namespace smartsocket
