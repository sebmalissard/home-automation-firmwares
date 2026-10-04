# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(TempSensor)

idf_build_set_property(CXX_COMPILE_OPTIONS "-std=gnu++17;-Os;-DCHIP_HAVE_CONFIG_H;-Wno-overloaded-virtual" APPEND)
idf_build_set_property(C_COMPILE_OPTIONS "-Os" APPEND)
# For RISCV chips, project_include.cmake sets -Wno-format, but does not clear various
# flags that depend on -Wformat
idf_build_set_property(COMPILE_OPTIONS "-Wno-format-nonliteral;-Wno-format-security" APPEND)

# Workaround for nixy4/u8g2 on ESP-IDF 6.x: the component includes
# <driver/gpio.h> but doesn't require esp_driver_gpio. Remove once fixed upstream.
idf_component_get_property(u8g2_lib nixy4__u8g2 COMPONENT_LIB)
idf_component_get_property(gpio_lib esp_driver_gpio COMPONENT_LIB)
target_link_libraries(${u8g2_lib} PRIVATE ${gpio_lib})
