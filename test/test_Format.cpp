// Tests for the integer-only LCD text helpers.
#include "../SmartSocket/src/core/Format.h"

#include "TestFramework.h"

using namespace smartsocket;

TEST(amps_renders_two_decimals) {
  char buf[10];
  format::amps(1240, buf, sizeof(buf));
  CHECK_STR_EQ(buf, "1.24");

  format::amps(0, buf, sizeof(buf));
  CHECK_STR_EQ(buf, "0.00");

  format::amps(2000, buf, sizeof(buf));
  CHECK_STR_EQ(buf, "2.00");

  format::amps(85, buf, sizeof(buf));
  CHECK_STR_EQ(buf, "0.09");  // rounded, not truncated
}

TEST(amps_rounds_rather_than_truncating) {
  char buf[10];

  // 1.996 A must not display as 1.99: the carry has to propagate into the whole
  // part.
  format::amps(1996, buf, sizeof(buf));
  CHECK_STR_EQ(buf, "2.00");

  format::amps(999, buf, sizeof(buf));
  CHECK_STR_EQ(buf, "1.00");
}

TEST(amps_clamps_negative_readings) {
  char buf[10];
  // A sensor reading below its zero point is noise, not a negative current.
  format::amps(-500, buf, sizeof(buf));
  CHECK_STR_EQ(buf, "0.00");
}

TEST(duration_renders_hh_mm_ss) {
  char buf[10];
  format::duration(0, buf, sizeof(buf));
  CHECK_STR_EQ(buf, "00:00:00");

  format::duration(8133000, buf, sizeof(buf));
  CHECK_STR_EQ(buf, "02:15:33");

  format::duration(59000, buf, sizeof(buf));
  CHECK_STR_EQ(buf, "00:00:59");
}

TEST(duration_saturates_instead_of_wrapping) {
  char buf[10];
  // A two-digit hour field cannot show 150 h. Saturating is honest; wrapping to
  // "06" would be a lie.
  format::duration(150u * 3600u * 1000u, buf, sizeof(buf));
  CHECK_STR_EQ(buf, "99:00:00");
}

TEST(duration_short_is_compact) {
  char buf[10];
  format::durationShort(45u * 60u * 1000u, buf, sizeof(buf));
  CHECK_STR_EQ(buf, "0h45m");

  format::durationShort((12u * 3600u + 30u * 60u) * 1000u, buf, sizeof(buf));
  CHECK_STR_EQ(buf, "12h30m");
}

TEST(duration_short_handles_a_four_digit_hour_count_at_its_documented_minimum) {
  // Regression: the guard allowed for one byte too few, so a large total ran off
  // the end of the caller's array. totalSavedMs near uint32 max is ~1193 hours.
  // The bug hid because every existing caller happened to pass a roomier buffer
  // than the header asked for.
  char exact[9];
  const uint8_t written = format::durationShort(0xFFFFFFFFu, exact, sizeof(exact));
  CHECK_EQ(written, 8);
  CHECK_STR_EQ(exact, "1193h02m");
}

TEST(duration_short_refuses_a_buffer_that_is_one_byte_short) {
  char tooSmall[8];
  CHECK_EQ(format::durationShort(0xFFFFFFFFu, tooSmall, sizeof(tooSmall)), 0);
}

TEST(number_renders_without_padding) {
  char buf[10];
  format::number(0, buf, sizeof(buf));
  CHECK_STR_EQ(buf, "0");

  format::number(7, buf, sizeof(buf));
  CHECK_STR_EQ(buf, "7");

  format::number(1234567, buf, sizeof(buf));
  CHECK_STR_EQ(buf, "1234567");
}

TEST(formatters_refuse_to_overflow_a_short_buffer) {
  char buf[4];
  // Better to write nothing than to run off the end of the caller's array.
  CHECK_EQ(format::amps(1240, buf, sizeof(buf)), 0);
  CHECK_EQ(format::duration(1000, buf, sizeof(buf)), 0);
  CHECK_EQ(format::number(123456, buf, sizeof(buf)), 0);
}

TEST(state_names_fit_beside_a_current_reading) {
  // The status line is stateName + " 12.34A" on 16 columns, so a name longer
  // than 8 chars would push the number off the display.
  for (int i = State_Calibrating; i <= State_ManualOff; ++i) {
    const char* name = format::stateName(static_cast<SocketState>(i));
    int len = 0;
    while (name[len] != '\0') {
      ++len;
    }
    CHECK(len <= 8);
  }
}

TEST(pad_field_pads_and_truncates) {
  char buf[9];
  format::padField("ab", buf, 8);
  buf[8] = '\0';
  CHECK_STR_EQ(buf, "ab      ");

  format::padField("abcdefghijk", buf, 8);
  buf[8] = '\0';
  CHECK_STR_EQ(buf, "abcdefgh");
}
