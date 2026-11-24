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
find_package(Threads REQUIRED)

# ---------------------------------------------------------------- protocolo UART (C99, el mismo del firmware)
# common/protocolo trae su propio CMakeLists: aqui solo se usa la biblioteca "protocolo" (sus pruebas
# Unity se compilan cuando ese directorio es el proyecto principal; ver la CI).
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../common/protocolo ${CMAKE_BINARY_DIR}/protocolo)

# ---------------------------------------------------------------- nucleo (C++17 puro)
# Sin CUDA, TensorRT, V4L2, OpenCV ni red: compila y se prueba en cualquier PC (y en la CI).
add_library(epp_core STATIC
    src/core/decidir_multicuadro.cpp
    src/core/maquina_estados.cpp
    src/core/preprocesamiento.cpp
    src/core/posprocesamiento.cpp
    src/core/imagen_ppm.cpp
    src/core/config.cpp
    src/core/enlace.cpp
    src/core/instrumentacion.cpp
    src/core/http.cpp
    src/core/api.cpp
)
target_include_directories(epp_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(epp_core PUBLIC protocolo)
target_compile_options(epp_core PRIVATE ${EPP_AVISOS})

# ---------------------------------------------------------------- SQLite (registro de eventos)
# find_package(SQLite3) no existe en CMake 3.10: se busca a mano.
find_path(SQLITE3_INCLUDE_DIR sqlite3.h)
find_library(SQLITE3_LIB sqlite3)
if(NOT SQLITE3_INCLUDE_DIR OR NOT SQLITE3_LIB)
  message(FATAL_ERROR "Falta SQLite: sudo apt install libsqlite3-dev (Ubuntu/Jetson) o sudo dnf install sqlite-devel (Fedora)")
endif()

# ---------------------------------------------------------------- plataforma: camara, puerto serie, SQLite, HTTP, /proc
# Solo dependen de POSIX/kernel y SQLite: se compilan tambien en la CI para detectar errores.
add_library(epp_plataforma STATIC
    src/platform/captura_v4l2.cpp
    src/platform/puerto_serie.cpp
    src/platform/registro_sqlite.cpp
    src/platform/servidor_http.cpp
    src/platform/sistema_linux.cpp)
target_include_directories(epp_plataforma PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/platform ${SQLITE3_INCLUDE_DIR})
target_link_libraries(epp_plataforma PUBLIC epp_core ${SQLITE3_LIB} Threads::Threads)
target_compile_options(epp_plataforma PRIVATE ${EPP_AVISOS})

# ---------------------------------------------------------------- plataforma: TensorRT (solo Jetson)
find_path(CUDA_INCLUDE_DIR cuda_runtime_api.h HINTS /usr/local/cuda/include)
find_library(CUDART_LIB cudart HINTS /usr/local/cuda/lib64)
find_path(TENSORRT_INCLUDE_DIR NvInfer.h HINTS /usr/include/aarch64-linux-gnu)
find_library(NVINFER_LIB nvinfer HINTS /usr/lib/aarch64-linux-gnu)

if(CUDA_INCLUDE_DIR AND CUDART_LIB AND TENSORRT_INCLUDE_DIR AND NVINFER_LIB)
  message(STATUS "TensorRT y CUDA encontrados: se compila epp-daemon")
  add_executable(epp-daemon src/main.cpp src/platform/inferencia_tensorrt.cpp)
  target_include_directories(epp-daemon PRIVATE ${CUDA_INCLUDE_DIR} ${TENSORRT_INCLUDE_DIR})
  target_link_libraries(epp-daemon PRIVATE epp_plataforma epp_core ${NVINFER_LIB} ${CUDART_LIB} Threads::Threads)
  target_compile_options(epp-daemon PRIVATE ${EPP_AVISOS})
  install(TARGETS epp-daemon DESTINATION bin)
else()
  message(STATUS "TensorRT/CUDA no encontrados: epp-daemon se omite (normal en un PC); se compilan nucleo, plataforma y pruebas")
endif()

# ---------------------------------------------------------------- pruebas (GoogleTest del sistema)
include(CTest)
if(BUILD_TESTING)
  # No FetchContent (no existe en CMake 3.10). En la Jetson usar -DBUILD_TESTING=OFF.
  find_package(GTest REQUIRED)
  add_executable(epp_tests
      tests/test_main.cpp
      tests/test_decidir_multicuadro.cpp
      tests/test_maquina_estados.cpp
      tests/test_preprocesamiento.cpp
      tests/test_posprocesamiento.cpp
      tests/test_protocolo.cpp
      tests/test_config.cpp
      tests/test_enlace.cpp
      tests/test_puerto_serie.cpp
      tests/test_instrumentacion.cpp
      tests/test_api.cpp
      tests/test_registro.cpp
      tests/test_servidor_http.cpp)
  target_include_directories(epp_tests PRIVATE ${GTEST_INCLUDE_DIRS})
  target_link_libraries(epp_tests PRIVATE epp_plataforma epp_core ${GTEST_LIBRARIES} Threads::Threads)
  add_test(NAME epp_tests COMMAND epp_tests)
endif()
