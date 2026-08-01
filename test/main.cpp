// Test entry point. Test cases self-register via the TEST macro, so this file
// never needs touching when tests are added.
#include <cstdio>

#include "TestFramework.h"

int main() {
  std::printf("\n=====================================\n");
  std::printf("  Smart Socket - core test suite\n");
  std::printf("=====================================\n\n");
  return testing::runAll();
}
