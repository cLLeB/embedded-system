// Fakes.h - stand-ins for every piece of hardware.
//
// These are what let a three-hour charge session be tested in microseconds: the
// FakeClock advances by assignment rather than by waiting, so config::TaperConfirmMs
// (90 s) costs nothing to cross.
#ifndef SMARTSOCKET_TEST_FAKES_H
#define SMARTSOCKET_TEST_FAKES_H

#include <cstring>
#include <string>
#include <vector>

#include "../SmartSocket/src/core/Interfaces.h"
#include "../SmartSocket/src/core/ProfileCodec.h"

namespace smartsocket {
namespace fakes {

class FakeClock : public IClock {
 public:
  FakeClock() : nowMs_(0) {}

  Millis now() const { return nowMs_; }

  void advance(Millis deltaMs) { nowMs_ += deltaMs; }
  void set(Millis ms) { nowMs_ = ms; }

 private:
  Millis nowMs_;
};

class FakeCurrentSensor : public ICurrentSensor {
 public:
  FakeCurrentSensor() : value_(0), calibrateCalls_(0), readCalls_(0) {}

  void calibrateZero() { ++calibrateCalls_; }

  Milliamps read() {
    ++readCalls_;
    return value_;
  }

  void setCurrent(Milliamps ma) { value_ = ma; }
  int calibrateCalls() const { return calibrateCalls_; }
  int readCalls() const { return readCalls_; }

 private:
  Milliamps value_;
  int calibrateCalls_;
  int readCalls_;
};

class FakeRelay : public IRelay {
 public:
  FakeRelay() : closed_(false), transitions_(0) {}

  void setClosed(bool closed) {
    if (closed != closed_) {
      ++transitions_;
    }
    closed_ = closed;
    history_.push_back(closed);
  }

  bool isClosed() const { return closed_; }
  int transitions() const { return transitions_; }
  const std::vector<bool>& history() const { return history_; }

 private:
  bool closed_;
  int transitions_;
  std::vector<bool> history_;
};

class FakeDisplay : public IDisplay {
 public:
  FakeDisplay() : beginCalls_(0) {
    lines_[0] = "";
    lines_[1] = "";
  }

  void begin() { ++beginCalls_; }

  void showLine(uint8_t row, const char* text) {
    if (row < 2) {
      lines_[row] = text;
    }
  }

  const std::string& line(uint8_t row) const { return lines_[row < 2 ? row : 0]; }
  int beginCalls() const { return beginCalls_; }

 private:
  std::string lines_[2];
  int beginCalls_;
};

class FakeBuzzer : public IBuzzer {
 public:
  FakeBuzzer() : sounding_(false), edges_(0) {}

  void setSounding(bool sounding) {
    if (sounding != sounding_) {
      ++edges_;
    }
    sounding_ = sounding;
  }

  bool isSounding() const { return sounding_; }
  int edges() const { return edges_; }

 private:
  bool sounding_;
  int edges_;
};

class FakeButtonSource : public IButtonSource {
 public:
  FakeButtonSource() {
    for (int i = 0; i < Button_Count; ++i) {
      pressed_[i] = false;
    }
  }

  bool isPressed(ButtonId id) const {
    return id < Button_Count ? pressed_[id] : false;
  }

  void press(ButtonId id) { pressed_[id] = true; }
  void release(ButtonId id) { pressed_[id] = false; }

 private:
  bool pressed_[Button_Count];
};

// Backed by a real byte array run through the real codec, so tests exercise the
// actual CRC and framing rather than a shortcut that stores a struct directly.
class FakeProfileStore : public IProfileStore {
 public:
  FakeProfileStore() : hasData_(false), saveCalls_(0) {
    std::memset(bytes_, 0xFF, sizeof(bytes_));
  }

  bool load(DeviceProfile& out) {
    if (!hasData_) {
      return false;
    }
    return profile_codec::decode(bytes_, out);
  }

  void save(const DeviceProfile& profile) {
    ++saveCalls_;
    hasData_ = true;
    profile_codec::encode(profile, bytes_);
  }

  int saveCalls() const { return saveCalls_; }

  // Simulates EEPROM corruption.
  void corrupt(uint8_t index) {
    hasData_ = true;
    bytes_[index] ^= 0xFF;
  }

  void seed(const DeviceProfile& profile) {
    hasData_ = true;
    profile_codec::encode(profile, bytes_);
  }

 private:
  uint8_t bytes_[profile_codec::RecordSize];
  bool hasData_;
  int saveCalls_;
};

}  // namespace fakes
}  // namespace smartsocket

#endif  // SMARTSOCKET_TEST_FAKES_H
