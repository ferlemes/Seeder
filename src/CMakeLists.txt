file(GLOB_RECURSE app_sources "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
idf_component_register(SRCS ${app_sources} INCLUDE_DIRS "." REQUIRES framework-arduinoespressif32)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unknown-pragmas)
