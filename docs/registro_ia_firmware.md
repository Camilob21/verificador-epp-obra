# Registro de uso de IA: firmware ESP32-C6 y protocolo

**Herramienta:** Claude (Anthropic), modelo Claude Opus 5.5, sesión del 23 de noviembre de 2026.
**Qué generó la IA:** `common/protocolo/` (biblioteca y pruebas), `firmware/esp32c6/` (firmware, Kconfig, sdkconfig, simulador `tools/sim_jetson.py`) y los documentos `docs/protocolo_uart.md`, `docs/firmware_*.md` y este registro.
**Qué decidieron los estudiantes:** requisitos, arquitectura general, formato inicial de la trama, elección de placa y componentes. *(Completen ustedes.)*

## 1. Lo que la IA verificó en su propio entorno (sin hardware)

| Afirmación | Cómo se verificó | Resultado |
|---|---|---|
| CRC-16/CCITT-FALSE("123456789") = 0x29B1 | Prueba PU-01 (C) y `assert` en el simulador (Python) | Correcto |
| Las 18 pruebas unitarias pasan | gcc 64-bit + Unity 2.6.1 + CTest | 18/18 |
| Sin errores de memoria | valgrind `--leak-check=full` | 0 errores, 0 fugas |
| Sin comportamiento indefinido | ASan + UBSan | Sin reportes |
| La biblioteca compila con warnings estrictos | gcc y clang con `-Wall -Wextra -Wpedantic -Wconversion -Werror`; el header compila como C++17 | OK |
| El firmware compila con ESP-IDF v5.5.6 para esp32c6 | CMake + Ninja con la toolchain oficial (riscv32-esp-elf GCC 14.2), `-Werror` en `main` | Binario de 0x2be60 B, 83 % libre en la partición |
| La variante USB + buzzer pasivo + relé activo en alto también compila | Segunda compilación con esas opciones | OK |
| El sdkconfig aplica las opciones (watchdog 2 s, estadísticas de CPU, consola solo en UART0) | Revisión del `sdkconfig` generado | OK |
| El simulador Python y la biblioteca C se entienden | Emulador del ESP32 en el PC (usa la biblioteca C real) conectado al simulador por un pseudo-terminal | 19/19 pruebas `--auto` |
| Los pines elegidos no son de arranque, USB ni consola | Guía de usuario oficial de la ESP32-C6-DevKitC-1 (tablas J1 y J3) | Ver README, sección 1 |
| Valores de `esp_reset_reason_t` | Archivo `esp_system.h` de ESP-IDF v5.5.6 | Coinciden con la tabla del protocolo |

**Limitación importante:** para compilar, la IA instaló las dependencias de Python de ESP-IDF a mano, desde sus repositorios de GitHub, porque su entorno no tenía acceso a PyPI. La compilación es válida, pero **debe repetirse con una instalación normal en su máquina** (guía, Parte 0).

## 2. Lo que la IA NO pudo verificar (lo deben verificar ustedes)

| Pendiente | Dónde se verifica |
|---|---|
| Que el firmware corra en la placa real | Guía, partes 1–5 |
| Modelo y polaridad del relé; si IN tiene 5 V | Guía 2.3–2.4, HW-01, HW-06 |
| Tipo de buzzer | Guía 2.1 |
| Que la salida del PIR sea ≤ 3,6 V; calentamiento real | Guía 2.5–2.6 |
| Estado del GPIO19 durante el reset y el bootloader | HW-01 |
| Consumo desde el pin 5V de la DevKitC-1 | Observar si hay reinicios por brownout |
| Tamaños de stack adecuados | Telemetría después de las pruebas (HW-10) |
| Que `/dev/ttyTHS1` = pines 8/10 en JetPack 4.6.1 y que `nvgetty` interfiere | Guía 6.2 y 6.4. Fuente: foros de NVIDIA y JetsonHacks (no es documentación oficial) |
| Paquetes de la Parte 0 de la guía | Tomados de la guía "Get Started" de Espressif; no probados en su distribución |
| Resistencias de los LEDs | Dependen de la Vf real de sus LEDs |

## 3. Decisiones de diseño propuestas por la IA (para revisar)

1. LEN máxima de 48 B (en lugar de 32) para que quepa la telemetría de 42 B.
2. Fail-safe en 800 ms, en vez de "más de 1 s", para cumplir de verdad el criterio de "rojo en ≤ 1 s".
3. Nuevos mensajes EVENTO_ARRANQUE y EVENTO_ENLACE, y NACK con código.
4. Protección local: VERDE máximo 8 s.
5. Watchdog de tareas de 2 s.
6. Resincronización que vuelve a examinar los bytes después de un falso inicio.
7. Transporte alternativo por el USB nativo, para probar sin adaptador USB-UART.
8. Prueba de lámparas al arrancar, y distinción visual entre "nunca conectó" (rojo fijo) y "perdió el enlace" (rojo con apagón).
9. Pruebas unitarias compiladas con gcc en el host, en lugar del target "linux" de ESP-IDF.

## 4. Fuentes consultadas

- Espressif: ESP32-C6-DevKitC-1 User Guide (tablas de pines J1/J3, pines de arranque, LED RGB en GPIO8, USB en GPIO12/13).
- Espressif: esptool, "Boot Mode Selection" (GPIO8/GPIO9).
- Espressif: ESP-IDF, "USB Serial/JTAG Controller Console" (ESP32-C6).
- Foros de NVIDIA Developer y JetsonHacks: UART del J41 (pines 8/10, `/dev/ttyTHS1`, `nvgetty`).
- Código fuente de ESP-IDF v5.5.6 (`esp_system.h`, ejemplo `hello_world`, `install.sh`).
