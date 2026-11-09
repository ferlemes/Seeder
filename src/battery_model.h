#pragma once
#include <stdint.h>

namespace battery {
enum class Source { Unknown, Battery, External5V };
struct Reading {
  Source source;
  uint16_t millivolts;
  uint8_t estimatedPercent;
};
inline Reading interpret(uint16_t mv) {
  if (mv > 4350) return { Source::External5V, mv, 0 };
  if (mv < 2500 || mv > 4250) return { Source::Unknown, mv, 0 };
  // Coarse voltage estimate only, rounded to 10%; not a fuel gauge.
  const uint16_t bounded = mv < 3300 ? 3300 : (mv > 4200 ? 4200 : mv);
  const uint8_t percent = ((bounded - 3300 + 45) / 90) * 10;
  return { Source::Battery, mv, percent };
}
}
