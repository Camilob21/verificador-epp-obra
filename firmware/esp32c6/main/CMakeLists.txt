idf_component_register(
    SRCS "main.c" "salidas.c" "enlace.c" "entradas.c" "telemetria.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES protocolo esp_driver_gpio esp_driver_uart esp_driver_ledc
                  esp_driver_usb_serial_jtag esp_timer
)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
