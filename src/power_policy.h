#pragma once
#include <stdint.h>

// Pure input/timer policy; no retained state, hardware or wallet data.
class PowerPolicy {
public:
  enum Action { Run, Block, Restore, IdleWarning, ManualWarning, Sleep };
  static constexpr uint32_t timeoutMs = 300000;
  static constexpr uint32_t warningMs = 30000;
  static constexpr uint32_t holdMs = 3000;
  static constexpr uint32_t releaseMs = 30;

  void begin(uint32_t now) {
    lastActivity = now;
    waitingRelease = true;
    releaseTracking = warning = chord = sleepPending = false;
  }

  Action update(uint32_t now, bool move, bool select) {
    const bool pressed = move || select;
    if (waitingRelease) {
      if (pressed) {
        releaseTracking = false;
        lastActivity = now;
        return sleepPending ? ManualWarning : Block;
      }
      if (!releaseTracking) { releasedAt = now; releaseTracking = true; }
      if (uint32_t(now - releasedAt) < releaseMs)
        return sleepPending ? ManualWarning : Block;
      waitingRelease = false;
      lastActivity = now;
      return sleepPending ? Sleep : Restore;
    }

    if (chord) {
      lastActivity = now;
      if (move && select) {
        if (uint32_t(now - chordAt) >= holdMs) {
          sleepPending = waitingRelease = true;
          releaseTracking = false;
        }
        return ManualWarning;
      }
      // A cancelled chord must not become a coin, roll or navigation click.
      chord = false;
      waitingRelease = true;
      releaseTracking = false;
      return Block;
    }

    if (warning) {
      if (pressed) {
        warning = false;
        waitingRelease = true;
        releaseTracking = false;
        lastActivity = now;
        return Block;
      }
      return uint32_t(now - lastActivity) >= timeoutMs ? Sleep : IdleWarning;
    }
    if (move && select) {
      chord = true;
      chordAt = lastActivity = now;
      return ManualWarning;
    }
    if (pressed) { lastActivity = now; return Run; }
    const uint32_t idle = now - lastActivity;
    if (idle >= timeoutMs) return Sleep;
    if (idle >= timeoutMs - warningMs) { warning = true; return IdleWarning; }
    return Run;
  }

  uint8_t secondsRemaining(uint32_t now, bool manual) const {
    const uint32_t elapsed = now - (manual ? chordAt : lastActivity);
    const uint32_t limit = manual ? holdMs : timeoutMs;
    return elapsed >= limit ? 0 : (limit - elapsed + 999) / 1000;
  }

private:
  uint32_t lastActivity = 0, chordAt = 0, releasedAt = 0;
  bool waitingRelease = true, releaseTracking = false;
  bool warning = false, chord = false, sleepPending = false;
};
