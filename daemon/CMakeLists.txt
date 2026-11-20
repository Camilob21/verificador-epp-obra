cmake_minimum_required(VERSION 3.10)
project(epp_daemon LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Nucleo sin dependencias de CUDA, TensorRT, V4L2, OpenCV ni red.
add_library(epp_core STATIC
    src/core/decision.cpp
    ../common/protocolo/protocolo.c
)
target_include_directories(epp_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/protocolo
)

add_executable(epp-daemon src/main.cpp)
target_link_libraries(epp-daemon PRIVATE epp_core)

include(CTest)
if(BUILD_TESTING)
    # No FetchContent (no esta disponible en CMake 3.10).
    # Acepta el libgtest-dev/gmock de la distribucion.
    find_package(Threads REQUIRED)
    find_package(GTest REQUIRED)
    add_executable(epp_tests tests/test_main.cpp tests/test_decision.cpp tests/test_protocolo.cpp)
    target_include_directories(epp_tests PRIVATE ${GTEST_INCLUDE_DIRS})
    target_link_libraries(epp_tests PRIVATE epp_core ${GTEST_LIBRARIES} Threads::Threads)
    add_test(NAME epp_tests COMMAND epp_tests)
endif()
