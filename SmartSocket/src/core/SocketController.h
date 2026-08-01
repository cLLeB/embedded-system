// SocketController.h - the state machine that owns the relay.
//
// Consumes an ICurrentSensor, drives an IRelay, and delegates the full-charge
// decision to ChargeAnalyzer. Knows nothing about pins or displays.
//
// Portable C++: must not include Arduino.h.
#ifndef SMARTSOCKET_CORE_SOCKETCONTROLLER_H
#define SMARTSOCKET_CORE_SOCKETCONTROLLER_H

#include "ChargeAnalyzer.h"
#include "Interfaces.h"
#include "Types.h"

namespace smartsocket {

// A snapshot of everything the UI needs. Returned by value so the presenter cannot
// reach into the controller's internals.
struct SocketStatus {
  SocketState state;
  Milliamps currentMa;
  Milliamps peakMa;
  Milliamps thresholdMa;
  Millis sessionElapsedMs;
  uint16_t taperRatioPct;
  uint16_t cutoffCount;
  uint32_t totalSavedMs;
  bool relayClosed;
};

class SocketController {
 public:
  SocketController(IClock& clock, ICurrentSensor& sensor, IRelay& relay,
                   IProfileStore& store);

  // Loads the learned profile, opens the relay, and calibrates the sensor zero.
  // The relay is opened FIRST: calibration is only valid with no current flowing.
  void begin();

  // Steps the machine. Safe to call every loop iteration; internally rate-limited
  // to config::SampleIntervalMs.
  void update();

  void onButton(ButtonId id, ButtonEvent event);

  // A command from a phone or a serial monitor. Deliberately routed through the
  // same transitions the buttons use, so a remote client can never reach a state
  // the panel cannot, and no safety rule has a second implementation.
  void onRemote(RemoteCommand command);

  SocketStatus status() const;
  SocketState state() const { return state_; }

  // Reports the buzzer pattern requested by the most recent state change, once.
  // Returns false if nothing changed since the last call.
  //
  // Silence is delivered as a real Buzz_Silent command rather than by returning
  // false, because some patterns (Fault) never end on their own: acknowledging a
  // fault has to actively stop the buzzer.
  bool takeAlert(BuzzerPattern& out);

 private:
  void enter(SocketState next);
  void handleVerdict(ChargeVerdict verdict);
  void learnFromSession();

  // Back to Cutoff after a probe found nothing. Distinct from enter(State_Cutoff)
  // because that announces a cutoff, and a probe that found nothing is not a new
  // cutoff - re-announcing it would mean three beeps every interval, all night,
  // for one charge that finished hours ago.
  void returnToCutoffQuietly();

  IClock& clock_;
  ICurrentSensor& sensor_;
  IRelay& relay_;
  IProfileStore& store_;
  ChargeAnalyzer analyzer_;

  DeviceProfile profile_;
  SocketState state_;
  Millis lastSampleMs_;
  Millis stateEnteredMs_;
  Millis plugSeenSinceMs_;
  Milliamps lastCurrentMa_;

  // How long to wait in Cutoff before the next probe. Chosen when the cutoff
  // happens, from whether anything was still drawing at that moment.
  Millis probeIntervalMs_;

  // Current has been seen while the contacts are commanded open. Held across
  // samples so one noisy reading cannot raise a fault on its own.
  Millis relaySuspectSinceMs_;
  bool relaySuspect_;

  BuzzerPattern pendingAlert_;
  bool alertPending_;
  bool plugCandidate_;
};

}  // namespace smartsocket

#endif  // SMARTSOCKET_CORE_SOCKETCONTROLLER_H
