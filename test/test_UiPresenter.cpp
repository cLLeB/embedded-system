// Tests for LCD rendering. Every line must be exactly 16 characters: the display
// is never cleared between frames, so a short line would leave stale characters
// from the previous screen visible.
#include "../SmartSocket/src/core/UiPresenter.h"

#include "TestFramework.h"

using namespace smartsocket;

namespace {

SocketStatus makeStatus(SocketState state) {
  SocketStatus s;
  s.state = state;
  s.currentMa = 1240;
  s.peakMa = 2000;
  s.thresholdMa = 600;
  s.sessionElapsedMs = 8133000;  // 02:15:33
  s.taperRatioPct = 30;
  s.cutoffCount = 7;
  s.totalSavedMs = (12u * 3600u + 30u * 60u) * 1000u;
  s.relayClosed = true;
  return s;
}

int lengthOf(const char* s) {
  int n = 0;
  while (s[n] != '\0') {
    ++n;
  }
  return n;
}

}  // namespace

TEST(status_screen_shows_state_and_live_current) {
  UiPresenter ui;
  char l0[LineBufferSize];
  char l1[LineBufferSize];

  ui.render(makeStatus(State_Charging), l0, l1);

  CHECK_STR_EQ(l0, "CHARGING   1.24A");
  CHECK_STR_EQ(l1, "02:15:33  pk2.00");
}

TEST(detail_screen_shows_the_cutoff_threshold) {
  UiPresenter ui;
  char l0[LineBufferSize];
  char l1[LineBufferSize];

  ui.nextScreen();
  CHECK_EQ(ui.screen(), Screen_Detail);
  ui.render(makeStatus(State_Charging), l0, l1);

  CHECK_STR_EQ(l0, "Cut at     0.60A");
  CHECK_STR_EQ(l1, "Taper 30%    n=7");
}

TEST(stats_screen_shows_lifetime_totals) {
  UiPresenter ui;
  char l0[LineBufferSize];
  char l1[LineBufferSize];

  ui.nextScreen();
  ui.nextScreen();
  CHECK_EQ(ui.screen(), Screen_Stats);
  ui.render(makeStatus(State_Charging), l0, l1);

  CHECK_STR_EQ(l0, "Cutoffs        7");
  CHECK_STR_EQ(l1, "Saved     12h30m");
}

TEST(screens_cycle_back_to_the_start) {
  UiPresenter ui;
  CHECK_EQ(ui.screen(), Screen_Status);
  ui.nextScreen();
  ui.nextScreen();
  ui.nextScreen();
  CHECK_EQ(ui.screen(), Screen_Status);
}

TEST(cutoff_takes_over_the_display_whatever_screen_was_selected) {
  UiPresenter ui;
  char l0[LineBufferSize];
  char l1[LineBufferSize];

  // Leaving a stats page up while power is cut would be the display hiding the
  // one thing the user has to act on.
  ui.nextScreen();
  ui.nextScreen();
  ui.render(makeStatus(State_Cutoff), l0, l1);

  CHECK_STR_EQ(l0, "FULL - POWER CUT");
  CHECK_STR_EQ(l1, "ACTION=check now");
}

TEST(fault_takes_over_the_display) {
  UiPresenter ui;
  char l0[LineBufferSize];
  char l1[LineBufferSize];

  ui.render(makeStatus(State_Fault), l0, l1);
  CHECK_STR_EQ(l0, "! OVERCURRENT ! ");
  // Must say tap, not hold: holding ACTION is the manual-off toggle, which from
  // a fault drops the user into MANUAL POWER OFF rather than re-arming.
  CHECK_STR_EQ(l1, "Tap ACTION      ");
}

TEST(manual_off_takes_over_the_display) {
  UiPresenter ui;
  char l0[LineBufferSize];
  char l1[LineBufferSize];

  ui.render(makeStatus(State_ManualOff), l0, l1);
  CHECK_STR_EQ(l0, "MANUAL POWER OFF");
  CHECK_STR_EQ(l1, "Hold ACTION=ON  ");
}

TEST(every_line_is_exactly_sixteen_characters) {
  UiPresenter ui;
  char l0[LineBufferSize];
  char l1[LineBufferSize];

  const SocketState states[] = {State_Calibrating, State_Idle,   State_Settling,
                                State_Charging,    State_Cutoff, State_Fault,
                                State_ManualOff};

  for (int s = 0; s < 7; ++s) {
    for (int screen = 0; screen < Screen_Count; ++screen) {
      ui.render(makeStatus(states[s]), l0, l1);
      CHECK_EQ(lengthOf(l0), config::LcdColumns);
      CHECK_EQ(lengthOf(l1), config::LcdColumns);
      ui.nextScreen();
    }
  }
}

TEST(large_values_do_not_overflow_the_line) {
  UiPresenter ui;
  char l0[LineBufferSize];
  char l1[LineBufferSize];

  SocketStatus s = makeStatus(State_Charging);
  s.currentMa = 4999;
  s.peakMa = 5000;
  s.cutoffCount = 65535;
  s.totalSavedMs = 0xFFFFFFFFu;

  for (int screen = 0; screen < Screen_Count; ++screen) {
    ui.render(s, l0, l1);
    CHECK_EQ(lengthOf(l0), config::LcdColumns);
    CHECK_EQ(lengthOf(l1), config::LcdColumns);
    ui.nextScreen();
  }
}
