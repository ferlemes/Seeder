#include <Arduino.h>
#include "battery.h"

namespace battery {
static Reading current = {Source::Unknown, 0, 0};
static uint32_t lastSample;
#if defined(SEEDER_BOARD_TDISPLAY_S3)
static void sample() {
  uint32_t sum = 0;
  for (uint8_t i=0; i<16; ++i) sum += analogReadMilliVolts(4);
  // Official schematic: equal 100k resistors, sensing the selected power rail.
  current = interpret(static_cast<uint16_t>((sum * 2 + 8) / 16));
  lastSample = millis();
}
#endif
void begin() {
#if defined(SEEDER_BOARD_TDISPLAY_S3)
  analogReadResolution(12);
  analogSetPinAttenuation(4, ADC_11db);
  sample();
#endif
}
void tick() {
#if defined(SEEDER_BOARD_TDISPLAY_S3)
  if (uint32_t(millis() - lastSample) >= 5000) sample();
#endif
}
Reading reading() { return current; }
}
