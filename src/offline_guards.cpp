#include "sdkconfig.h"
#if defined(SEEDER_OFFLINE)
#if defined(CONFIG_IDF_TARGET_ESP32) && (!defined(CONFIG_ESP32_NO_BLOBS) || CONFIG_ESP32_NO_BLOBS != 1)
#error "Offline firmware requires ESP32_NO_BLOBS"
#endif
#if defined(CONFIG_IDF_TARGET_ESP32S3) && (!defined(CONFIG_SEEDER_NO_RADIO) || CONFIG_SEEDER_NO_RADIO != 1)
#error "Offline ESP32-S3 firmware requires explicit radio component exclusion"
#endif
#if defined(CONFIG_ESP32_WIFI_ENABLED) || defined(CONFIG_BT_ENABLED) || defined(CONFIG_SW_COEXIST_ENABLE) || defined(CONFIG_ESP32_WIFI_SW_COEXIST_ENABLE)
#error "Radio support must be disabled"
#endif
#if defined(CONFIG_SECURE_BOOT) || defined(CONFIG_SECURE_FLASH_ENC_ENABLED) || defined(CONFIG_ESP32_DISABLE_BASIC_ROM_CONSOLE)
#error "Offline build must not burn security eFuses"
#endif
#endif
