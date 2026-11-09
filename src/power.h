#pragma once
namespace power {
void begin();
// True means the power UI consumed this iteration; do not process buttons.
bool tick();
}
