#include "SocketController.h"

#include "Config.h"

namespace smartsocket {

SocketController::SocketController(IClock& clock, ICurrentSensor& sensor,
                                   IRelay& relay, IProfileStore& store)
    : clock_(clock),
      sensor_(sensor),
      relay_(relay),
      store_(store),
      state_(State_Calibrating),
      lastSampleMs_(0),
      stateEnteredMs_(0),
      plugSeenSinceMs_(0),
      lastCurrentMa_(0),
      probeIntervalMs_(config::ProbeEmptyIntervalMs),
      relaySuspectSinceMs_(0),
      relaySuspect_(false),
      pendingAlert_(Buzz_Silent),
      alertPending_(false),
      plugCandidate_(false) {
  profile_.learnedTaperPct = config::TaperRatioDefaultPct;
  profile_.lastPeakMa = 0;
  profile_.cutoffCount = 0;
  profile_.totalSavedMs = 0;
}

void SocketController::begin() {
  // Fail safe: the relay is opened before anything else, so a reset mid-charge
  // never leaves the load energized while we are still working things out. It also
  // guarantees zero current, which is what makes the calibration below valid.
  relay_.setClosed(false);
  state_ = State_Calibrating;
  stateEnteredMs_ = clock_.now();

  DeviceProfile loaded;
  if (store_.load(loaded)) {
    profile_ = loaded;
  }
  // A stored ratio outside the sane band is an artefact, not a device trait.
  if (profile_.learnedTaperPct < config::TaperRatioMinPct ||
      profile_.learnedTaperPct > config::TaperRatioMaxPct) {
    profile_.learnedTaperPct = config::TaperRatioDefaultPct;
  }

  // Close the contacts BEFORE calibrating. The zero offset has to be measured
  // under the conditions the socket actually runs in, and it runs with the relay
  // closed - the coil's own 70 mA through the shared 5 V rail is part of the
  // measurement, not something to be excluded from it. Calibrating with the
  // relay open produced 80-100 mA of phantom current on an empty socket.
  //
  // Safe with something already plugged in: the calibration averages over whole
  // mains cycles, so an AC load cancels instead of being zeroed away. See
  // HalCurrentSensorBase::calibrateZero.
  relay_.setClosed(true);
  sensor_.calibrateZero();

  enter(State_Idle);
  lastSampleMs_ = clock_.now();
}

void SocketController::enter(SocketState next) {
  state_ = next;
  stateEnteredMs_ = clock_.now();

  // Every branch below names a buzzer pattern, including silence. Leaving one
  // unset would let a previous pattern run on into the new state - and since the
  // Fault pattern never ends by itself, that would mean a socket that keeps
  // beeping after the fault is cleared.
  alertPending_ = true;

  switch (next) {
    case State_Idle:
      // Power must be ON while idle: current is the only way to notice a device
      // being plugged in, and an open relay would make the socket blind.
      relay_.setClosed(true);
      plugCandidate_ = false;
      lastCurrentMa_ = 0;
      pendingAlert_ = Buzz_Silent;
      break;

    case State_Settling:
      relay_.setClosed(true);
      pendingAlert_ = Buzz_PlugDetected;
      break;

    case State_Charging:
      relay_.setClosed(true);
      analyzer_.beginSession(clock_.now(), profile_.learnedTaperPct);
      pendingAlert_ = Buzz_Silent;
      break;

    case State_Cutoff:
      relay_.setClosed(false);
      pendingAlert_ = Buzz_Cutoff;

      // Decide now, while the last reading still means something: was a device
      // sitting there drawing, or was the socket effectively empty? Once the
      // contacts open there is no way to ask again.
      probeIntervalMs_ = (lastCurrentMa_ >= config::ProbeEmptyBelowMa)
                             ? config::ProbeChargedIntervalMs
                             : config::ProbeEmptyIntervalMs;
      break;

    case State_Probing:
      // Contacts closed, but this is not a session: nothing is learned, no
      // counter moves, and the saved-time clock stops because power is on.
      relay_.setClosed(true);
      pendingAlert_ = Buzz_Silent;
      break;

    case State_Fault:
      relay_.setClosed(false);
      pendingAlert_ = Buzz_Fault;
      break;

    case State_RelayStuck:
      // Commanding it open again will not help - it was already commanded open,
      // which is how we got here - but it costs nothing and covers the case
      // where the drive, not the contacts, is what failed.
      relay_.setClosed(false);
      relaySuspect_ = false;
      pendingAlert_ = Buzz_Fault;
      break;

    case State_ManualOff:
      relay_.setClosed(false);
      pendingAlert_ = Buzz_Silent;
      break;

    case State_Calibrating:
    default:
      relay_.setClosed(false);
      pendingAlert_ = Buzz_Silent;
      break;
  }
}

void SocketController::learnFromSession() {
  const uint16_t observed = analyzer_.observedTaperPct();
  if (observed == 0) {
    return;  // no usable peak; nothing to learn from
  }

  // A session that never reached a real charging current was an already-full
  // device, not a charge. Its taper ratio is ~100% - meaningless as a device
  // characteristic, and learning from it would drag the stored ratio upward and
  // start cutting real charges off early.
  if (analyzer_.peakMa() < config::MinSessionPeakMa) {
    return;
  }

  uint16_t clamped = observed;
  if (clamped < config::TaperRatioMinPct) {
    clamped = config::TaperRatioMinPct;
  } else if (clamped > config::TaperRatioMaxPct) {
    clamped = config::TaperRatioMaxPct;
  }

  // Exponential moving average: let each session nudge the learned value rather
  // than overwrite it, so one weird charge cannot retrain the socket.
  const uint32_t w = config::LearningWeightPct;
  profile_.learnedTaperPct = static_cast<uint16_t>(
      ((static_cast<uint32_t>(profile_.learnedTaperPct) * (100u - w)) +
       (static_cast<uint32_t>(clamped) * w)) /
      100u);

  const Milliamps peak = analyzer_.peakMa();
  profile_.lastPeakMa = static_cast<uint16_t>(peak < 0 ? 0 : peak);
  if (profile_.cutoffCount < 0xFFFF) {
    profile_.cutoffCount++;
  }
  store_.save(profile_);
}

void SocketController::handleVerdict(ChargeVerdict verdict) {
  switch (verdict) {
    case Verdict_Full:
      learnFromSession();
      enter(State_Cutoff);
      break;
    case Verdict_Unplugged:
      enter(State_Idle);
      break;
    case Verdict_Overcurrent:
      enter(State_Fault);
      break;
    case Verdict_Continue:
    default:
      break;
  }
}

void SocketController::update() {
  const Millis now = clock_.now();

  // Rate-limit sampling. Subtraction handles millis() rollover correctly because
  // the arithmetic is unsigned.
  if ((now - lastSampleMs_) < config::SampleIntervalMs) {
    return;
  }
  lastSampleMs_ = now;

  // Cutoff holds power off, and the relay stays open, so there is nothing to
  // read. Time spent here is what "saved" means.
  //
  // It does not hold forever, though. Every probeIntervalMs_ the contacts close
  // for a few seconds so the socket can find out whether anything has changed -
  // see the Probing case below and the note on ProbeDurationMs in Config.h.
  if (state_ == State_Cutoff) {
    profile_.totalSavedMs += config::SampleIntervalMs;

    // Sample even though the relay is open. With the contacts parted this must
    // read zero; anything else means they did not part, and the socket is
    // claiming on its display to have cut power that is still flowing.
    lastCurrentMa_ = sensor_.read();
    const Millis inCutoff = now - stateEnteredMs_;

    if (inCutoff >= config::RelayStuckGraceMs) {
      if (lastCurrentMa_ >= config::RelayStuckMa) {
        if (!relaySuspect_) {
          relaySuspect_ = true;
          relaySuspectSinceMs_ = now;
        } else if ((now - relaySuspectSinceMs_) >=
                   config::RelayStuckConfirmMs) {
          enter(State_RelayStuck);
          return;
        }
      } else {
        relaySuspect_ = false;
      }
    }

    if (inCutoff >= probeIntervalMs_) {
      enter(State_Probing);
    }
    return;
  }

  // Fault, RelayStuck and ManualOff genuinely do require a human. A fault means
  // something is wrong that a machine should not silently retry, and manual off
  // is a direct instruction - probing past any of them would override the user.
  if (state_ == State_Fault || state_ == State_RelayStuck ||
      state_ == State_ManualOff || state_ == State_Calibrating) {
    return;
  }

  const Milliamps ma = sensor_.read();
  lastCurrentMa_ = ma;

  // Protection is checked here, on the RAW reading, before the state switch - so
  // it applies in every state the relay can be closed in, not just Charging.
  //
  // Two things this must not do: it must not live inside the analyzer (which only
  // runs while Charging, leaving an over-limit load connected for the several
  // seconds it takes to cross Idle and Settling), and it must not be tested
  // against the smoothed value (the EMA takes seconds to climb, and a protection
  // limit that waits for a filter is not protection).
  if (ma > config::ImplausibleMa || ma > config::OvercurrentMa) {
    enter(State_Fault);
    return;
  }

  switch (state_) {
    case State_Idle: {
      if (ma >= config::PlugDetectMa) {
        if (!plugCandidate_) {
          plugCandidate_ = true;
          plugSeenSinceMs_ = now;
        } else if ((now - plugSeenSinceMs_) >= config::SampleIntervalMs) {
          // Two consecutive samples above the plug threshold: a real load, not a
          // single noisy reading.
          enter(State_Settling);
        }
      } else {
        plugCandidate_ = false;
      }
      break;
    }

    case State_Settling: {
      // Chargers pull a large inrush spike and negotiate power for the first few
      // seconds. Sampling that as the peak would poison the baseline and push the
      // taper threshold far too high, so the whole window is discarded.
      if (ma < config::UnplugMa) {
        enter(State_Idle);
      } else if ((now - stateEnteredMs_) >= config::SettleMs) {
        enter(State_Charging);
      }
      break;
    }

    case State_Charging: {
      handleVerdict(analyzer_.update(now, ma));
      break;
    }

    case State_Probing: {
      // The bar is a real charging current, not merely a present load. A device
      // that was just cut for being full still draws something while it runs, and
      // resuming on that would put the socket straight back into the trickle it
      // just escaped.
      if (ma >= config::ProbeResumeMa) {
        enter(State_Settling);
      } else if ((now - stateEnteredMs_) >= config::ProbeDurationMs) {
        returnToCutoffQuietly();
      }
      break;
    }

    default:
      break;
  }
}

void SocketController::returnToCutoffQuietly() {
  const Millis interval = probeIntervalMs_;
  enter(State_Cutoff);

  // enter() picked a fresh interval from the current reading, but that reading
  // was taken with the relay closed for a probe, not at the end of a charge.
  // Keep the interval the real cutoff chose.
  probeIntervalMs_ = interval;

  pendingAlert_ = Buzz_Silent;
}

void SocketController::onButton(ButtonId id, ButtonEvent event) {
  if (event == ButtonEvent_None) {
    return;
  }

  if (id == Button_Action) {
    if (event == ButtonEvent_LongPress) {
      // Long press is the manual override toggle, available from any state.
      if (state_ == State_ManualOff) {
        enter(State_Idle);
      } else {
        enter(State_ManualOff);
      }
      return;
    }

    // Short press acknowledges and re-arms. enter(State_Idle) silences the buzzer
    // as part of the transition.
    if (state_ == State_Cutoff || state_ == State_Fault ||
        state_ == State_RelayStuck || state_ == State_ManualOff) {
      // Leaving a cutoff is the moment to persist the time saved: it has been
      // accumulating since the relay opened, and the count is only final now.
      // Saving it here costs one EEPROM write per session, where saving on every
      // tick would burn through the cells in days.
      if (state_ == State_Cutoff) {
        store_.save(profile_);
      }
      enter(State_Idle);
    }
  }
}

bool SocketController::takeAlert(BuzzerPattern& out) {
  if (!alertPending_) {
    return false;
  }
  alertPending_ = false;
  out = pendingAlert_;
  return true;
}

SocketStatus SocketController::status() const {
  SocketStatus s;
  s.state = state_;
  s.currentMa = lastCurrentMa_;

  // The analyzer is only reset on entry to Charging, so outside that state its
  // peak and threshold still hold the previous session's numbers. Reporting them
  // would show the last charge's figures as if they were live.
  const bool sessionLive = (state_ == State_Charging);
  s.peakMa = sessionLive ? analyzer_.peakMa() : 0;
  s.thresholdMa = sessionLive ? analyzer_.taperThresholdMa() : 0;
  s.sessionElapsedMs = sessionLive ? analyzer_.elapsedMs(clock_.now()) : 0;
  s.taperRatioPct = profile_.learnedTaperPct;
  s.cutoffCount = profile_.cutoffCount;
  s.totalSavedMs = profile_.totalSavedMs;
  s.relayClosed = relay_.isClosed();
  return s;
}

}  // namespace smartsocket
