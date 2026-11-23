cmake_minimum_required(VERSION 3.10)   # Ubuntu 18.04 (JetPack 4.6.1) trae CMake 3.10.2
project(epp_daemon LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)   # en la Jetson el preprocesamiento sin -O2 es varias veces mas lento
endif()
set(EPP_AVISOS -Wall -Wextra)

# ---------------------------------------------------------------- nucleo (C++17 puro)
# Sin CUDA, TensorRT, V4L2, OpenCV ni red: compila y se prueba en cualquier PC (y en la CI).
add_library(epp_core STATIC
    src/core/decidir_multicuadro.cpp
    src/core/maquina_estados.cpp
    src/core/preprocesamiento.cpp
    src/core/posprocesamiento.cpp
    src/core/imagen_ppm.cpp
)
target_include_directories(epp_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(epp_core PRIVATE ${EPP_AVISOS})

# ---------------------------------------------------------------- protocolo UART (C99, compartido con el ESP32)
# BLOQUE PROVISIONAL: usa el borrador actual de common/protocolo. Se reemplaza cuando entre la
# biblioteca del firmware (common/protocolo con su propio CMakeLists.txt), por:
#   add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../common/protocolo ${CMAKE_BINARY_DIR}/protocolo)
#   y enlazar el destino "protocolo" en lugar de epp_protocolo.
add_library(epp_protocolo STATIC ../common/protocolo/protocolo.c)
target_include_directories(epp_protocolo PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../common/protocolo)

# ---------------------------------------------------------------- plataforma: camara V4L2
# Solo depende de las cabeceras del kernel: se compila tambien en la CI para detectar errores.
add_library(epp_v4l2 STATIC src/platform/captura_v4l2.cpp)
target_include_directories(epp_v4l2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/platform)
target_link_libraries(epp_v4l2 PUBLIC epp_core)
target_compile_options(epp_v4l2 PRIVATE ${EPP_AVISOS})

# ---------------------------------------------------------------- plataforma: TensorRT (solo Jetson)
find_path(CUDA_INCLUDE_DIR cuda_runtime_api.h HINTS /usr/local/cuda/include)
find_library(CUDART_LIB cudart HINTS /usr/local/cuda/lib64)
find_path(TENSORRT_INCLUDE_DIR NvInfer.h HINTS /usr/include/aarch64-linux-gnu)
find_library(NVINFER_LIB nvinfer HINTS /usr/lib/aarch64-linux-gnu)

if(CUDA_INCLUDE_DIR AND CUDART_LIB AND TENSORRT_INCLUDE_DIR AND NVINFER_LIB)
  message(STATUS "TensorRT y CUDA encontrados: se compila epp-daemon")
  add_executable(epp-daemon src/main.cpp src/platform/inferencia_tensorrt.cpp)
  target_include_directories(epp-daemon PRIVATE ${CUDA_INCLUDE_DIR} ${TENSORRT_INCLUDE_DIR})
  target_link_libraries(epp-daemon PRIVATE epp_v4l2 epp_core ${NVINFER_LIB} ${CUDART_LIB})
  target_compile_options(epp-daemon PRIVATE ${EPP_AVISOS})
else()
  message(STATUS "TensorRT/CUDA no encontrados: epp-daemon se omite (normal en un PC); se compilan nucleo, camara y pruebas")
endif()

# ---------------------------------------------------------------- pruebas (GoogleTest del sistema)
include(CTest)
if(BUILD_TESTING)
  # No FetchContent (no existe en CMake 3.10). En la Jetson usar -DBUILD_TESTING=OFF.
  find_package(Threads REQUIRED)
  find_package(GTest REQUIRED)
  add_executable(epp_tests
      tests/test_main.cpp
      tests/test_decidir_multicuadro.cpp
      tests/test_maquina_estados.cpp
      tests/test_preprocesamiento.cpp
      tests/test_posprocesamiento.cpp
      tests/test_protocolo.cpp)
  target_include_directories(epp_tests PRIVATE ${GTEST_INCLUDE_DIRS})
  target_link_libraries(epp_tests PRIVATE epp_core epp_protocolo ${GTEST_LIBRARIES} Threads::Threads)
  add_test(NAME epp_tests COMMAND epp_tests)
endif()
