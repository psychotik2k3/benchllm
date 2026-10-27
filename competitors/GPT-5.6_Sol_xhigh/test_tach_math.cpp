#include <assert.h>
#include <math.h>
#include <stdint.h>

#include "tach_math.h"

static bool nearlyEqual(float actual, float expected, float tolerance) {
  return fabsf(actual - expected) <= tolerance;
}

int main() {
  // Two tach pulses per revolution:
  // 3000 RPM -> 100 Hz -> 10000 us between falling edges.
  assert(nearlyEqual(TachMath::rpmFromPeriodUs(10000.0f), 3000.0f, 0.01f));

  // 1600 RPM -> 53.333 Hz -> 18750 us between falling edges.
  assert(nearlyEqual(TachMath::rpmFromPeriodUs(18750.0f), 1600.0f, 0.01f));

  // 3000 RPM multiplied by 2.5 -> 250 Hz -> 2000 us half-period.
  assert(TachMath::outputHalfPeriodUs(10000.0f, 2.5f) == 2000U);

  // 1600 RPM multiplied by 2.0 -> 106.667 Hz -> 4687.5 us half-period.
  assert(TachMath::outputHalfPeriodUs(18750.0f, 2.0f) == 4688U);

  assert(TachMath::ratioIsValid(0.10f));
  assert(TachMath::ratioIsValid(10.0f));
  assert(!TachMath::ratioIsValid(0.099f));
  assert(!TachMath::ratioIsValid(10.001f));
  assert(!TachMath::ratioIsValid(NAN));
  assert(TachMath::rpmFromPeriodUs(0.0f) == 0.0f);

  return 0;
}
