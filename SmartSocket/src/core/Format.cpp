#include "Format.h"

namespace smartsocket {
namespace format {
namespace {

// Writes `value` right-aligned into `width` digits, zero-padded. Assumes the caller
// has checked the buffer. Used for the fixed-width fields of HH:MM:SS.
void writeZeroPadded(uint32_t value, uint8_t width, char* out) {
  for (int8_t i = static_cast<int8_t>(width) - 1; i >= 0; --i) {
    out[i] = static_cast<char>('0' + (value % 10));
    value /= 10;
  }
}

}  // namespace

uint8_t number(uint32_t value, char* out, uint8_t outSize) {
  if (out == 0 || outSize == 0) {
    return 0;
  }

  // Count digits first so we can write left-to-right without reversing afterwards.
  uint8_t digits = 1;
  for (uint32_t probe = value; probe >= 10; probe /= 10) {
    ++digits;
  }
  if (digits + 1 > outSize) {
    out[0] = '\0';
    return 0;
  }

  writeZeroPadded(value, digits, out);
  out[digits] = '\0';
  return digits;
}

uint8_t amps(Milliamps ma, char* out, uint8_t outSize) {
  if (out == 0 || outSize < 8) {
    if (out != 0 && outSize > 0) {
      out[0] = '\0';
    }
    return 0;
  }

  if (ma < 0) {
    ma = 0;
  }

  const uint32_t whole = static_cast<uint32_t>(ma) / 1000u;
  // Round to 2 decimals rather than truncating, so 1.996 A reads 2.00 not 1.99.
  uint32_t hundredths = ((static_cast<uint32_t>(ma) % 1000u) + 5u) / 10u;
  uint32_t wholeAdjusted = whole;
  if (hundredths >= 100u) {
    hundredths -= 100u;
    wholeAdjusted += 1u;
  }

  uint8_t pos = number(wholeAdjusted, out, outSize);
  if (pos == 0 || static_cast<uint8_t>(pos + 4) > outSize) {
    out[0] = '\0';
    return 0;
  }
  out[pos++] = '.';
  writeZeroPadded(hundredths, 2, out + pos);
  pos = static_cast<uint8_t>(pos + 2);
  out[pos] = '\0';
  return pos;
}

uint8_t duration(Millis ms, char* out, uint8_t outSize) {
  if (out == 0 || outSize < 9) {
    if (out != 0 && outSize > 0) {
      out[0] = '\0';
    }
    return 0;
  }

  uint32_t totalSeconds = ms / 1000u;
  uint32_t hours = totalSeconds / 3600u;
  const uint32_t minutes = (totalSeconds % 3600u) / 60u;
  const uint32_t seconds = totalSeconds % 60u;

  // A two-digit field cannot show more than 99 hours; saturate rather than wrap to
  // a misleading small number.
  if (hours > 99u) {
    hours = 99u;
  }

  writeZeroPadded(hours, 2, out);
  out[2] = ':';
  writeZeroPadded(minutes, 2, out + 3);
  out[5] = ':';
  writeZeroPadded(seconds, 2, out + 6);
  out[8] = '\0';
  return 8;
}

uint8_t durationShort(Millis ms, char* out, uint8_t outSize) {
  if (out == 0 || outSize < 9) {
    if (out != 0 && outSize > 0) {
      out[0] = '\0';
    }
    return 0;
  }

  const uint32_t totalMinutes = ms / 60000u;
  const uint32_t hours = totalMinutes / 60u;
  const uint32_t minutes = totalMinutes % 60u;

  uint8_t pos = number(hours, out, outSize);
  // Still to write: 'h' + two digits + 'm' + terminator = pos + 5, not pos + 4.
  // (amps() needs pos + 4 because it has no trailing unit character; copying its
  // guard here would overrun by one byte on a 4-digit hour count.)
  if (pos == 0 || static_cast<uint8_t>(pos + 5) > outSize) {
    out[0] = '\0';
    return 0;
  }
  out[pos++] = 'h';
  writeZeroPadded(minutes, 2, out + pos);
  pos = static_cast<uint8_t>(pos + 2);
  out[pos++] = 'm';
  out[pos] = '\0';
  return pos;
}

const char* stateName(SocketState state) {
  switch (state) {
    case State_Calibrating:
      return "CALIB";
    case State_Idle:
      return "READY";
    case State_Settling:
      return "SETTLE";
    case State_Charging:
      return "CHARGING";
    case State_Cutoff:
      return "FULL";
    case State_Fault:
      return "FAULT";
    case State_ManualOff:
      return "OFF";
    case State_Probing:
      return "CHECKING";
    case State_RelayStuck:
      return "STUCK";
    default:
      return "?";
  }
}

void padField(const char* src, char* out, uint8_t width) {
  uint8_t i = 0;
  if (src != 0) {
    for (; i < width && src[i] != '\0'; ++i) {
      out[i] = src[i];
    }
  }
  for (; i < width; ++i) {
    out[i] = ' ';
  }
}

}  // namespace format
}  // namespace smartsocket
