#include <Arduino.h>
#include "boards/board.h"
#include "power.h"

#if defined(SEEDER_BOARD_TDISPLAY_S3)
#include <TFT_eSPI.h>
#include "driver/rtc_io.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "gpio.h"
#include "power_policy.h"
#include "workflow.h"
#include "ui/ui.h"

extern TFT_eSPI tft;
extern sButton btnMove;
extern sButton btnSelect;

namespace {
PowerPolicy policy;
PowerPolicy::Action lastAction = PowerPolicy::Run;
uint8_t lastSeconds = 255;
constexpr gpio_num_t wakePin = static_cast<gpio_num_t>(PIN_MOVE);
constexpr gpio_num_t peripheralPowerPin = static_cast<gpio_num_t>(PIN_POWER_ON);

void sleepNow() {
  // Configure wake before erasing data or shutting down the display.
  ESP_ERROR_CHECK(esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL));
  ESP_ERROR_CHECK(rtc_gpio_init(wakePin));
  ESP_ERROR_CHECK(rtc_gpio_set_direction(wakePin, RTC_GPIO_MODE_INPUT_ONLY));
  ESP_ERROR_CHECK(rtc_gpio_pullup_en(wakePin));
  ESP_ERROR_CHECK(rtc_gpio_pulldown_dis(wakePin));
  ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(wakePin, 0));

  wipeSeed();
  tft.fillScreen(TFT_BLACK);
  digitalWrite(TFT_BL, LOW);
  tft.writecommand(ST7789_DISPOFF);
  tft.writecommand(ST7789_SLPIN);
  delay(120);

  // Keep LCD peripheral power and backlight off throughout deep sleep.
  ESP_ERROR_CHECK(rtc_gpio_init(peripheralPowerPin));
  ESP_ERROR_CHECK(rtc_gpio_set_direction(peripheralPowerPin, RTC_GPIO_MODE_OUTPUT_ONLY));
  ESP_ERROR_CHECK(rtc_gpio_set_level(peripheralPowerPin, 0));
  ESP_ERROR_CHECK(rtc_gpio_hold_en(peripheralPowerPin));
  ESP_ERROR_CHECK(gpio_hold_en(static_cast<gpio_num_t>(TFT_BL)));
  gpio_deep_sleep_hold_en();
  esp_deep_sleep_start();
}
}

namespace power {
void begin() {
  // Release RTC/hold configuration before Init_TFT() restores GPIO15 and BL.
  gpio_deep_sleep_hold_dis();
  ESP_ERROR_CHECK(gpio_hold_dis(static_cast<gpio_num_t>(TFT_BL)));
  ESP_ERROR_CHECK(rtc_gpio_hold_dis(peripheralPowerPin));
  ESP_ERROR_CHECK(rtc_gpio_deinit(peripheralPowerPin));
  ESP_ERROR_CHECK(rtc_gpio_deinit(wakePin));
  btnMove.init();
  btnSelect.init();
  policy.begin(millis());
}

bool tick() {
  const uint32_t now = millis();
  const auto action = policy.update(now, digitalRead(PIN_MOVE) == LOW,
                                        digitalRead(PIN_SELECT) == LOW);
  if (action == PowerPolicy::Sleep) sleepNow();
  if (action == PowerPolicy::Restore) {
    btnMove.init();
    btnSelect.init();
    redrawCurrentScreen();
  } else if (action == PowerPolicy::IdleWarning || action == PowerPolicy::ManualWarning) {
    const bool manual = action == PowerPolicy::ManualWarning;
    const uint8_t seconds = policy.secondsRemaining(now, manual);
    if (lastAction != action || lastSeconds != seconds) {
      if (manual) ui::powerManualWarning(seconds);
      else ui::powerIdleWarning(seconds);
    }
    lastSeconds = seconds;
  }
  lastAction = action;
  return action != PowerPolicy::Run;
}
}
#else
namespace power {
void begin() {}
bool tick() { return false; }
}
#endif
