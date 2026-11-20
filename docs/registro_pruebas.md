# Registro de pruebas y afirmaciones

Regla: distinguir **reportado por el equipo**, **calculado aquí** y **observado en hardware real**.

| Fecha | Afirmación | Cómo se verificó | Resultado | Estado / responsable |
|---|---|---|---|---|
| 2026-11-20 | Modelo YOLOv8n mAP50 0,782 | Reporte suministrado en conversación | 0,782 reportado | No reproducido en este repositorio |
| 2026-11-20 | CMake de Ubuntu 18.04 ~3.10.2 | Consulta al paquete distribuido (no al Jetson) | CMake mínimo configurado a 3.10 | Comprobar versión en Jetson |
| 2026-11-20 | Daemon C++17 en Linux x86 | `cmake .. -DBUILD_TESTING=OFF`, `cmake --build .`, `./epp-daemon` | Compila y muestra `DENEGADO` | Verificado en contenedor Debian/GCC 14, no en Jetson |
| 2026-11-20 | CRC y regla básica verificables en x86 | Arnés temporal `assert` compilado con g++ contra `libepp_core.a` | 8 verificaciones pasan | Verificado fuera de Google Test; requiere CTest oficial |
| 2026-11-20 | Pruebas Google Test oficiales | `ctest` con `libgtest-dev` | **No ejecutadas** (Google Test ausente en entorno) | Pendiente en Fedora/CI |
| 2026-11-20 | Firmware ESP-IDF compila en ESP32-C6 | `idf.py build` en máquina con toolchain | **No ejecutado** | Pendiente |
| 2026-11-20 | GitHub Actions compila y ejecuta tests | Página Actions / log de primer push | **No ejecutado** | Pendiente |

## Registro de nueva ejecución

| Fecha/hora y sistema | Requisito/afirmación | Comando o instrumento | Salida real resumida | Resultado | Implementa | Verifica |
|---|---|---|---|---|---|---|
| (llenar) | | | | Pasa / Falla / No probado | | |
