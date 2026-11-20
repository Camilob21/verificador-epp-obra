cmake_minimum_required(VERSION 3.16)
# Reutilizar componentes del firmware real, no duplicar protocolo.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../components")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(epp_unity_tests)
