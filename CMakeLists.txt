cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(COMPONENTS src framework-arduinoespressif32 __pio_env)
project(seeder_offline)
