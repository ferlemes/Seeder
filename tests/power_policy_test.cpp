#include "../src/power_policy.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>

using P = PowerPolicy;
static P awake(uint32_t now=0) {
  P p; p.begin(now);
  assert(p.update(now, false, false) == P::Block);
  assert(p.update(now+30, false, false) == P::Restore);
  assert(p.update(now+31, false, false) == P::Run);
  return p;
}

int main() {
  { // Real five-minute deadline and thirty-second warning.
    auto p=awake();
    assert(p.update(270029, false, false)==P::Run);
    assert(p.update(270030, false, false)==P::IdleWarning);
    assert(p.secondsRemaining(270030, false)==30);
    assert(p.update(299030, false, false)==P::IdleWarning);
    assert(p.secondsRemaining(299030, false)==1);
    assert(p.update(300030, false, false)==P::Sleep);
  }
  for (int button=0; button<3; ++button) {
    // Either button (or both) dismisses the warning without leaking an input.
    auto p=awake();
    assert(p.update(270030,false,false)==P::IdleWarning);
    assert(p.update(280000,button!=1,button!=0)==P::Block);
    assert(p.update(280010,false,false)==P::Block);
    assert(p.update(280039,false,false)==P::Block);
    assert(p.update(280040,false,false)==P::Restore);
    assert(p.update(300030,false,false)==P::Run);
    assert(p.update(580040,false,false)==P::Sleep);
  }
  { // Normal interaction and a held button prevent inactivity sleep.
    auto p=awake();
    assert(p.update(269000,true,false)==P::Run);
    assert(p.update(300030,true,false)==P::Run);
    assert(p.update(570029,false,false)==P::Run);
    assert(p.update(570030,false,false)==P::IdleWarning);
  }
  { // A cancel press at the deadline takes priority over automatic sleep.
    auto p=awake();
    assert(p.update(270030,false,false)==P::IdleWarning);
    assert(p.update(300030,true,false)==P::Block);
    assert(p.update(300040,false,false)==P::Block);
    assert(p.update(300070,false,false)==P::Restore);
  }
  { // Manual sleep waits for both buttons to be released and debounced.
    auto p=awake();
    assert(p.update(100,true,true)==P::ManualWarning);
    assert(p.update(3099,true,true)==P::ManualWarning);
    assert(p.update(3100,true,true)==P::ManualWarning);
    assert(p.secondsRemaining(3100,true)==0);
    assert(p.update(9000,true,false)==P::ManualWarning);
    assert(p.update(9010,false,false)==P::ManualWarning);
    assert(p.update(9039,false,false)==P::ManualWarning);
    assert(p.update(9040,false,false)==P::Sleep);
  }
  { // Cancelling manual sleep consumes the whole chord, including its tail.
    auto p=awake();
    assert(p.update(100,true,true)==P::ManualWarning);
    assert(p.update(3099,false,true)==P::Block);
    assert(p.update(6000,false,true)==P::Block);
    assert(p.update(6010,false,false)==P::Block);
    assert(p.update(6040,false,false)==P::Restore);
    assert(p.update(6041,false,false)==P::Run);
  }
  { // Wake press and contact bounce cannot become an entropy/navigation input.
    P p; p.begin(0);
    assert(p.update(0,true,false)==P::Block);
    assert(p.update(10000,true,false)==P::Block);
    assert(p.update(10010,false,false)==P::Block);
    assert(p.update(10020,true,false)==P::Block);
    assert(p.update(10030,false,false)==P::Block);
    assert(p.update(10059,false,false)==P::Block);
    assert(p.update(10060,false,false)==P::Restore);
    assert(p.update(10061,false,false)==P::Run);
  }
  { // millis() overflow must not disable the timeout or shorten the hold.
    const uint32_t start=UINT32_MAX-100;
    auto p=awake(start);
    assert(p.update(start+270030u,false,false)==P::IdleWarning);
    assert(p.update(start+300030u,false,false)==P::Sleep);
    auto q=awake(start);
    assert(q.update(start+40u,true,true)==P::ManualWarning);
    assert(q.update(start+3040u,true,true)==P::ManualWarning);
    assert(q.update(start+3050u,false,false)==P::ManualWarning);
    assert(q.update(start+3080u,false,false)==P::Sleep);
  }
  puts("PASS: timeout, warning cancellation, input suppression, manual sleep, wake debounce, timer overflow");
}
