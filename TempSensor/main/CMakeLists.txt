idf_component_register(
    SRC_DIRS
        "."
        "utils"
    PRIV_INCLUDE_DIRS
        "."
        "utils"
    PRIV_REQUIRES
        esp_driver_gpio
        esp_driver_i2c
)

target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H")