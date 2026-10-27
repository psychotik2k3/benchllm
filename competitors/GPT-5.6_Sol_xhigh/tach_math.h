#pragma once

#include <math.h>
#include <stdint.h>

namespace TachMath {

constexpr float kPulsesPerRevolution = 2.0f;
constexpr float kMinRatio = 0.10f;
constexpr float kMaxRatio = 10.0f;

inline bool ratioIsValid(float ratio) {
  return isfinite(ratio) && ratio >= kMinRatio && ratio <= kMaxRatio;
}

inline float rpmFromPeriodUs(float tachPeriodUs) {
  if (!isfinite(tachPeriodUs) || tachPeriodUs <= 0.0f) {
    return 0.0f;
  }
  return 60000000.0f / (tachPeriodUs * kPulsesPerRevolution);
}

inline uint32_t outputHalfPeriodUs(float inputTachPeriodUs, float ratio) {
  if (!isfinite(inputTachPeriodUs) || inputTachPeriodUs <= 0.0f ||
      !ratioIsValid(ratio)) {
    return 0U;
  }

  const float halfPeriodUs = inputTachPeriodUs / (2.0f * ratio);
  return static_cast<uint32_t>(halfPeriodUs + 0.5f);
}

}  // namespace TachMath
