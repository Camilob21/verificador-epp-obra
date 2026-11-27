# Matriz de verificación: firmware ESP32-C6 y protocolo UART

> **Prototipo final:** no se montan el relé ni el buzzer; la autorización se indica con el LED verde. El firmware los conserva (relé en GPIO19, buzzer en GPIO20) como **mejora futura**, por eso esta documentación técnica los sigue describiendo. Las secciones y pruebas de relé y buzzer no aplican al prototipo.

**Roles** (complete con nombres): **Impl.** = quién implementa, **Prueba** = quién ejecuta la prueba. Se recomienda que quien prueba no sea quien implementó.
La columna **Resultado** se llena con PASA/FALLA, la fecha y la evidencia (captura, archivo o video).
Las filas marcadas con ✔ en "Ya verificado" se ejecutaron en el entorno de desarrollo de Claude (sin hardware) y **deben repetirse en su máquina**.

## A. Requisitos del firmware

| ID req. | Requisito |
|---|---|
| R-F01 | Al encender o reiniciar, el relé queda apagado y el LED rojo encendido hasta el primer latido válido |
| R-F02 | Si pasa más de 800 ms sin latido válido: rojo, relé apagado, estado SIN_ENLACE, en ≤ 1 s |
| R-F03 | Ejecutar ESTADO (5 estados) y BUZZER (5 patrones) recibidos de la Jetson |
| R-F04 | PIR filtrado, sin eventos durante el calentamiento; EVENTO_PIR en cada cambio |
| R-F05 | Botón mantenido ~5 s: patrón en los LEDs + EVENTO_BOTON (reintentos hasta recibir ACK) |
| R-F06 | Telemetría cada ~1 s y bajo pedido: heap, heap mínimo, stacks, CPU, contadores, uptime |
| R-F07 | Protocolo con CRC; el parser se resincroniza y cuenta los errores |
| R-F08 | El relé no queda en verde más de 8 s aunque la Jetson no lo ordene (protección local) |
| R-F09 | Si el firmware se cuelga, el watchdog reinicia el chip a estado seguro |
| R-F10 | Ningún pin del ESP32 recibe 5 V |

## B. Pruebas unitarias (protocolo, en el PC)

Herramientas: Unity 2.6.1 + CMake/CTest + gcc. Además: valgrind y ASan/UBSan.

| ID | Req. | Método | Criterio de aceptación | Impl. | Prueba | Ya verificado | Resultado |
|---|---|---|---|---|---|---|---|
| PU-01 | R-F07 | CRC de "123456789" | = 0x29B1 | | | ✔ | |
| PU-02 | R-F07 | CRC de entrada vacía; un bit cambiado | 0xFFFF; CRC distinto | | | ✔ | |
| PU-03 | R-F07 | Codificar un LATIDO | Bytes exactos esperados | | | ✔ | |
| PU-04 | R-F07 | Codificar con LEN > 48 o buffer pequeño | Devuelve 0 | | | ✔ | |
| PU-05 | R-F07 | Ida y vuelta de los 11 tipos, incluida la telemetría | Todos los campos coinciden | | | ✔ | |
| PU-06 | R-F07 | Endianness de la telemetría | uptime 0x11223344 → `44 33 22 11` | | | ✔ | |
| PU-07 | R-F07 | Trama con un dato corrupto | Rechazada, err_crc = 1 | | | ✔ | |
| PU-08 | R-F07 | CRC corrupto + trama válida | Se entrega solo la válida | | | ✔ | |
| PU-09 | R-F07 | Trama incompleta + timeout | No se entrega; err_timeout = 1; la siguiente sí se acepta | | | ✔ | |
| PU-10 | R-F07 | 10 bytes de basura sin 0xAA + trama | 1 trama, 10 bytes descartados | | | ✔ | |
| PU-11 | R-F07 | Basura con 0xAA falso + trama | La trama se recupera (resincronización) | | | ✔ | |
| PU-12 | R-F07 | Cabecera con LEN = 200 + trama | err_longitud = 1 y la trama se recupera | | | ✔ | |
| PU-13 | R-F07 | Entrega byte a byte y 3 tramas pegadas | 3 tramas en orden | | | ✔ | |
| PU-14 | R-F07 | 0xAA dentro de SEQ y de DATOS | Trama correcta | | | ✔ | |
| PU-15 | R-F07 | Tipo desconocido, LEN incorrecta, valores fuera de rango | Rechazados con el código correcto | | | ✔ | |
| PU-16 | R-F07 | 200 000 bytes aleatorios con 40 tramas intercaladas | Se recuperan las 40 | | | ✔ | |
| PU-17 | R-F07 | 200 000 bytes aleatorios (con 0xAA) | Sin fallos; < 50 tramas "válidas" por azar | | | ✔ | |
| PU-18 | R-F07 | Vectores publicados en docs/protocolo_uart.md | Bytes idénticos | | | ✔ | |
| PU-19 | R-F07 | Las pruebas bajo valgrind | 0 errores, 0 fugas | | | ✔ | |
| PU-20 | R-F07 | Las pruebas con ASan + UBSan | Sin reportes | | | ✔ | |
| PU-21 | R-F07 | Las pruebas en la Jetson (aarch64) | 18/18 | | | | |

## C. Pruebas de integración automáticas (firmware real + `tools/sim_jetson.py --auto`)

Herramienta: pyserial. Primero en modo USB (Parte 4 de la guía) y luego por el UART real con la Jetson (Parte 6).

| ID | Req. | Método | Criterio de aceptación | Impl. | Prueba | Resultado USB | Resultado UART real |
|---|---|---|---|---|---|---|---|
| PI-01 | R-F01 | ESTADO VERDE sin latido | NACK código 4 (SIN_ENLACE) | | | | |
| PI-02 | R-F01 | Iniciar latidos | EVENTO_ENLACE(1) en < 1 s | | | | |
| PI-03.0–4 | R-F03 | Los 5 estados | ACK; la telemetría muestra el mismo estado; relé ON solo en VERDE | | | | |
| PI-04.1–4 | R-F03 | 4 patrones de buzzer | ACK (sonido verificado a oído) | | | | |
| PI-05 | R-F03 | ESTADO = 9 | NACK código 3 | | | | |
| PI-06 | R-F07 | 5 tramas con CRC corrupto | err_crc aumenta ≥ 5 | | | | |
| PI-07 | R-F07 | Basura con 0xAA falso + ESTADO | ACK de la trama válida | | | | |
| PI-08 | R-F07 | Cabecera con LEN = 200 | err_longitud aumenta ≥ 1 | | | | |
| PI-09 | R-F02 | VERDE y cortar el latido | EVENTO_ENLACE(0) ≤ 1000 ms después del último latido; estado 5; relé OFF | | | | |
| PI-10 | R-F02 | Reanudar el latido | Enlace OK y estado ROJO (no vuelve a VERDE) | | | | |
| PI-11 | R-F08 | VERDE y esperar 8,5 s | Estado ROJO, relé OFF | | | | |
| PI-12 | R-F06 | Escuchar 3,5 s | Telemetría con periodo de 0,8–1,2 s | | | | |

*Nota sobre PI-09:* la medición incluye la latencia del USB y del sistema operativo del PC (unos pocos ms), así que el tiempo real del ESP32 es igual o menor. El simulador se probó contra un emulador del ESP32 en el PC (no contra la placa): dio 19/19, con 842 ms en PI-09.

## D. Pruebas de hardware y visuales

| ID | Req. | Método | Herramienta | Criterio de aceptación | Impl. | Prueba | Resultado |
|---|---|---|---|---|---|---|---|
| HW-01 | R-F01 | Desconectar y reconectar la alimentación 10 veces | Observación (LED del módulo, sonido del relé) | El relé no se activa en ningún encendido | | | |
| HW-02 | R-F03 | Recorrer los estados con el simulador | Video con el celular | Cada estado muestra su LED o patrón | | | |
| HW-03 | R-F01 | Encender sin la Jetson | Observación | Prueba de lámparas y luego rojo fijo | | | |
| HW-04 | R-F04 | Mover la mano frente al PIR (después de 60 s) | Simulador | Un evento por cambio, sin ráfagas; ninguno antes de 60 s | | | |
| HW-05 | R-F05 | Botón < 1 s / 3 s / 6 s | Simulador + observación | Nada / aviso y cancelación / EVENTO_BOTON + patrón + tono | | | |
| HW-06 | R-F10 | Medir IN del relé y OUT del PIR sin el ESP32 | Multímetro | ≤ 3,6 V en cualquier pin que vaya al ESP32 | | | |
| HW-07 | R-F02 | Cortar el latido en VERDE | Video en cámara lenta (240 fps) | El relé se apaga en ≤ 1 s | | | |
| HW-08 | R-F09 | Provocar un cuelgue (ver nota) | Consola + simulador | Reinicio; EVENTO_ARRANQUE con motivo 6 (watchdog de tareas); relé OFF | | | |
| HW-09 | R-F02 | Desconectar físicamente el cable TX de la Jetson con el sistema en VERDE | Observación | Rojo y relé OFF en ≤ 1 s | | | |
| HW-10 | R-F06 | Después de las pruebas, pedir telemetría | Simulador (`t`) | Ninguna tarea con stack libre < 512 B; heap mínimo > 100 kB | | | |

*Nota HW-08:* para provocar el cuelgue, compile temporalmente con un `while(1){}` dentro del `case PROTO_BUZZER` de `enlace.c`, envíe `b 1` y quite el cambio después. Es opcional, pero demuestra el watchdog.

## E. Herramientas usadas

| Herramienta | Versión | Uso |
|---|---|---|
| ESP-IDF | v5.5.6 | Compilación, flasheo, monitor |
| Unity | 2.6.1 (copiado en `common/protocolo/test/unity`, licencia MIT) | Pruebas unitarias |
| CMake / CTest | ≥ 3.10 | Construcción y ejecución de pruebas |
| valgrind | — | Errores de memoria y fugas |
| GCC/Clang sanitizers | — | ASan / UBSan |
| Python 3 + pyserial | ≥ 3.5 | Simulador de la Jetson y pruebas de integración |
| Multímetro, celular (cámara lenta) | — | Pruebas de hardware |
