#include "../src/battery_model.h"
#include <assert.h>
#include <stdio.h>
int main() {
  using namespace battery;
  assert(interpret(0).source==Source::Unknown);
  assert(interpret(2499).source==Source::Unknown);
  assert(interpret(3300).estimatedPercent==0);
  assert(interpret(3750).estimatedPercent==50);
  assert(interpret(4200).estimatedPercent==100);
  assert(interpret(4251).source==Source::Unknown);
  assert(interpret(4350).source==Source::Unknown);
  assert(interpret(4351).source==Source::External5V);
  assert(interpret(5000).source==Source::External5V);
  uint8_t previous=0;
  for (uint16_t mv=2500; mv<=4250; ++mv) {
    auto r=interpret(mv);
    assert(r.source==Source::Battery);
    assert(r.estimatedPercent>=previous && r.estimatedPercent<=100);
    assert(r.estimatedPercent%10==0);
    previous=r.estimatedPercent;
  }
  puts("PASS: battery estimate bounds/monotonicity, invalid readings and external 5V classification");
}
