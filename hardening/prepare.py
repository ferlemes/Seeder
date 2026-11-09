"""Build a minimal Arduino component from the pinned SDK sources in /tmp.

The original framework CMake/Kconfig are backed up next to the originals.
No RF libraries or network Arduino libraries are included by this component.
"""
Import("env")
from pathlib import Path
import os
os.environ["IDF_COMPONENT_MANAGER"] = "0"

framework = Path(env.PioPlatform().get_package_dir("framework-arduinoespressif32"))
for name in ("CMakeLists.txt", "Kconfig.projbuild"):
    path = framework / name
    backup = path.with_name(name + ".seeder-original")
    if not backup.exists():
        backup.write_bytes(path.read_bytes())

original = (framework / "CMakeLists.txt.seeder-original").read_text()
core = original.split("set(LIBRARY_SRCS", 1)[0]
minimal = '''
list(REMOVE_ITEM CORE_SRCS cores/esp32/esp32-hal-time.c)
set(offline_libs FS SPI SPIFFS)
set(includedirs variants/${CONFIG_ARDUINO_VARIANT} cores/esp32)
set(srcs ${CORE_SRCS})
foreach(lib IN LISTS offline_libs)
    file(GLOB lib_sources "${CMAKE_CURRENT_LIST_DIR}/libraries/${lib}/src/*.cpp")
    list(APPEND srcs ${lib_sources})
    list(APPEND includedirs "libraries/${lib}/src")
endforeach()
idf_component_register(SRCS ${srcs} INCLUDE_DIRS ${includedirs}
    PRIV_INCLUDE_DIRS cores/esp32/libb64
    REQUIRES spi_flash mbedtls esp_adc_cal nvs_flash app_update spiffs
             bootloader_support esp_ipc esp_timer driver vfs esp_event)
target_compile_options(${COMPONENT_LIB} PUBLIC
    -DARDUINO=10812 -DARDUINO_ARCH_ESP32
    -DARDUINO_BOARD="${CONFIG_ARDUINO_VARIANT}" -DARDUINO_VARIANT="${CONFIG_ARDUINO_VARIANT}" -DESP32)
target_link_libraries(${COMPONENT_LIB} INTERFACE "-u _Z5setupv -u _Z4loopv")
'''
(framework / "CMakeLists.txt").write_text(core + minimal)
kconfig = (framework / "Kconfig.projbuild.seeder-original").read_text()
start = kconfig.index("config ENABLE_ARDUINO_DEPENDS")
end = kconfig.index("config AUTOSTART_ARDUINO", start)
kconfig = kconfig[:start] + 'config ENABLE_ARDUINO_DEPENDS\n    bool\n    default n\n\n' + kconfig[end:]
(framework / "Kconfig.projbuild").write_text(kconfig)
idf = Path(env.PioPlatform().get_package_dir("framework-espidf"))
wifi = idf / "components/esp_wifi/Kconfig"
wifi_original = wifi.with_name("Kconfig.seeder-original")
if not wifi_original.exists():
    wifi_original.write_bytes(wifi.read_bytes())
wifi_text = wifi_original.read_text()
old = 'default "y" if !IDF_TARGET_ESP32H2'
assert old in wifi_text
wifi.write_text(wifi_text.replace(old, 'default "n"', 1))
wifi_cmake = wifi.parent / "CMakeLists.txt"
wifi_cmake_original = wifi_cmake.with_name("CMakeLists.txt.seeder-original")
if not wifi_cmake_original.exists():
    wifi_cmake_original.write_bytes(wifi_cmake.read_bytes())
(wifi.parent / "offline_disabled.c").write_text('/* No radio implementation. */\nvoid seeder_wifi_excluded(void) {}\n')
wifi_cmake.write_text('''if(NOT CONFIG_ESP32_WIFI_ENABLED)
    idf_component_register(SRCS "offline_disabled.c" INCLUDE_DIRS "include")
    return()
endif()
''' + wifi_cmake_original.read_text())
phy_dir = idf / "components/esp_phy"
phy_cmake = phy_dir / "CMakeLists.txt"
phy_original = phy_cmake.with_name("CMakeLists.txt.seeder-original")
if not phy_original.exists():
    phy_original.write_bytes(phy_cmake.read_bytes())
(phy_dir / "offline_disabled.c").write_text('/* No PHY implementation or replacement radio APIs. */\nvoid seeder_phy_excluded(void) {}\n')
phy_cmake.write_text('''if((CONFIG_SEEDER_NO_RADIO OR CONFIG_ESP32_NO_BLOBS) AND NOT CONFIG_ESP32_WIFI_ENABLED AND NOT CONFIG_BT_ENABLED)
    idf_component_register(SRCS "offline_disabled.c" INCLUDE_DIRS "include" "${IDF_TARGET}/include")
    return()
endif()
''' + phy_original.read_text())
# IDF 4.4 sleep code otherwise refers unconditionally to the excluded PHY.
# Keep all non-radio sleep workarounds and remove only the RF cleanup branch
# when this build has no radio implementation at all; do not add API stubs.
sleep_source = idf / "components/esp_hw_support/sleep_modes.c"
sleep_original = sleep_source.with_name("sleep_modes.c.seeder-original")
if not sleep_original.exists():
    sleep_original.write_bytes(sleep_source.read_bytes())
sleep_text = sleep_original.read_text()
old = '''        extern bool esp_phy_is_initialized(void);
        if (esp_phy_is_initialized()){
            extern void phy_close_rf(void);
            phy_close_rf();
#if !CONFIG_IDF_TARGET_ESP32
            extern void phy_xpd_tsens(void);
            phy_xpd_tsens();
#endif
        }'''
assert sleep_text.count(old) == 1
sleep_source.write_text(sleep_text.replace(old, '#if !CONFIG_SEEDER_NO_RADIO && !CONFIG_ESP32_NO_BLOBS\n' + old + '\n#endif'))
env.SConscript(str(Path(env.subst("$PROJECT_DIR")) / "scripts/version.py"), exports="env")
