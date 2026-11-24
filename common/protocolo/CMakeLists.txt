# CMakeLists.txt del protocolo: sirve para DOS sistemas de construcción.
#
# 1) Dentro de ESP-IDF (firmware/esp32c6 lo incluye vía EXTRA_COMPONENT_DIRS):
#    se registra como componente "protocolo" y termina aquí.
# 2) Fuera de ESP-IDF (PC con Fedora/Ubuntu, o la Jetson): biblioteca estática
#    + ejecutable de pruebas Unity, registrado en CTest.
#      cmake -S common/protocolo -B build-proto
#      cmake --build build-proto
#      ctest --test-dir build-proto --output-on-failure
#    El daemon C++ de la Jetson puede usarlo con add_subdirectory() y
#    target_link_libraries(... protocolo).

if(ESP_PLATFORM)
    idf_component_register(SRCS "src/protocolo.c"
                           INCLUDE_DIRS "include")
    return()
endif()

cmake_minimum_required(VERSION 3.10)
project(protocolo C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(protocolo STATIC src/protocolo.c)
target_include_directories(protocolo PUBLIC include)
target_compile_options(protocolo PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Werror)

# Pruebas solo si este directorio es el proyecto principal (no al usarlo desde el daemon).
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    option(PROTO_SANITIZERS "Compilar pruebas con AddressSanitizer y UBSan" OFF)
    enable_testing()
    add_executable(test_protocolo test/test_protocolo.c test/unity/unity.c)
    target_include_directories(test_protocolo PRIVATE test/unity)
    target_link_libraries(test_protocolo PRIVATE protocolo)
    if(PROTO_SANITIZERS)
        foreach(t protocolo test_protocolo)
            target_compile_options(${t} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -g)
            target_link_options(${t} PRIVATE -fsanitize=address,undefined)
        endforeach()
    endif()
    add_test(NAME protocolo COMMAND test_protocolo)
endif()
