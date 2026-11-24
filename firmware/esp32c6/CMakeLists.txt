# Proyecto ESP-IDF del firmware EPP para ESP32-C6.
# Construir:  idf.py set-target esp32c6   (solo la primera vez)
#             idf.py build
cmake_minimum_required(VERSION 3.16)

# El protocolo vive fuera del proyecto (common/protocolo) porque también lo usa
# el daemon de la Jetson. Se agrega como componente extra.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../common/protocolo")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(epp_esp32c6)
