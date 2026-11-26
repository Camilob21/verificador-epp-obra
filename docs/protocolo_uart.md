# Protocolo UART Jetson Nano ↔ ESP32-C6 (v1)

Implementación de referencia: `common/protocolo/` (C99, sin dependencias). La usan el firmware del ESP32 y el daemon de la Jetson. **Esta biblioteca es el contrato entre los dos lados**: si alguien cambia el formato, cambia la biblioteca, esta tabla y la prueba PU-18. No hay dos implementaciones distintas.

## 1. Capa física

| Parámetro | Valor |
|---|---|
| Interfaz | UART, lógica de 3,3 V en ambos lados |
| Velocidad | 115200 baudios, 8N1, sin control de flujo |
| ESP32-C6 | UART1: TX = GPIO10, RX = GPIO4 |
| Jetson Nano | Conector J41: pin 8 = TX, pin 10 = RX, pin 6 = GND → `/dev/ttyTHS1` |
| Cruce | TX de uno → RX del otro. **GND común obligatorio.** |

En la Jetson, el servicio `nvgetty` abre una consola en `/dev/ttyTHS1` y hay que desactivarlo (`sudo systemctl stop nvgetty` y `sudo systemctl disable nvgetty`). Además, al encender la Jetson manda una ráfaga de texto por el pin 8. El parser la descarta como basura (sección 5).

## 2. Formato de la trama

```
+------+------+------+------+------------------+--------+--------+
| 0xAA | TIPO | SEQ  | LEN  | DATOS (0..48 B)  | CRC lo | CRC hi |
+------+------+------+------+------------------+--------+--------+
         \______________ cubierto por el CRC __/
```

| Campo | Tamaño | Descripción |
|---|---|---|
| SOF | 1 B | Siempre `0xAA` |
| TIPO | 1 B | Tipo de mensaje (tabla 3) |
| SEQ | 1 B | Contador propio de cada emisor (0..255, da la vuelta). En ACK/NACK es el SEQ de la trama que se responde. |
| LEN | 1 B | Cantidad de bytes de DATOS, de 0 a 48. Cada tipo tiene una LEN fija. |
| DATOS | LEN B | Carga útil |
| CRC | 2 B | CRC-16/CCITT-FALSE sobre TIPO, SEQ, LEN y DATOS, **en little-endian** (primero el byte bajo) |

**CRC-16/CCITT-FALSE:** polinomio 0x1021, valor inicial 0xFFFF, sin reflexión, sin XOR final. Para comprobarlo: CRC("123456789") = **0x29B1** (prueba PU-01).

**Endianness:** todos los campos multibyte van en little-endian, incluido el CRC. Se serializan byte a byte (no se copian `struct`), así que el resultado no depende del procesador ni del compilador.

### Cambios respecto a la propuesta inicial y por qué

| Cambio | Razón |
|---|---|
| Máximo de DATOS: 32 → 48 B | La telemetría pedida (heap, 5 stacks, contadores, CPU, uptime) ocupa 42 B. |
| CRC en little-endian (antes sin definir) | Una sola regla de endianness para todo. |
| Nuevos EVENTO_ARRANQUE y EVENTO_ENLACE | La Jetson se entera de los reinicios del ESP32 (y del motivo: watchdog, brownout…). Además permite medir automáticamente el tiempo de reacción del fail-safe. |
| NACK con código | Para depurar: tipo desconocido, longitud, valor fuera de rango o "sin enlace". |
| Se mantienen SOF + LEN + CRC, sin COBS ni escape de bytes | Con la LEN y el CRC, un 0xAA dentro de los datos no causa problemas (prueba PU-14), y el parser es más simple de explicar y de probar. |

## 3. Mensajes

| Tipo | Nombre | Dirección | LEN | DATOS | Respuesta |
|---|---|---|---|---|---|
| 0x01 | LATIDO | J → E | 0 | — | ninguna |
| 0x02 | ESTADO | J → E | 1 | estado (tabla 4) | ACK o NACK |
| 0x03 | BUZZER | J → E | 1 | patrón (tabla 5) | ACK o NACK |
| 0x04 | PEDIR_TELEMETRIA | J → E | 0 | — | TELEMETRIA |
| 0x10 | ACK | ambas | 1 | tipo confirmado | — |
| 0x11 | NACK | ambas | 2 | tipo rechazado, código (tabla 6) | — |
| 0x80 | EVENTO_ARRANQUE | E → J | 3 | motivo de reinicio¹, fw mayor, fw menor | — |
| 0x81 | EVENTO_PIR | E → J | 2 | presencia (0/1), PIR listo (0/1) | — |
| 0x82 | EVENTO_BOTON | E → J | 1 | `1` = factory reset solicitado | **la Jetson debe responder ACK** |
| 0x83 | TELEMETRIA | E → J | 42 | ver sección 4 | — |
| 0x84 | EVENTO_ENLACE | E → J | 1 | 0 = latido perdido (fail-safe), 1 = restablecido | — |
| 0x85 | EVENTO_DISTANCIA | E → J | 3 | distancia en mm (u16 LE), válida (0/1) | — (periódico, ~4 por segundo; agregado el 24-nov para el HC-SR04) |

¹ Valor de `esp_reset_reason_t` en ESP-IDF v5.5: 1 = encendido, 3 = esp_restart, 4 = pánico, 5 = watchdog de interrupciones, 6 = watchdog de tareas, 9 = brownout.

**Tabla 4. Estados (ESTADO):** 0 REPOSO (todo apagado) · 1 AMARILLO_PARPADEO (colóquese en la marca) · 2 AMARILLO_FIJO (verificando) · 3 VERDE (relé activado) · 4 ROJO. El valor 5 = SIN_ENLACE solo aparece en la telemetría y no se puede ordenar.

**Tabla 5. Patrones de buzzer:** 0 silencio · 1 bip corto (100 ms) · 2 bip doble · 3 OK (600 ms) · 4 error (tres tonos).

**Tabla 6. Códigos de NACK:** 1 tipo desconocido · 2 longitud incorrecta · 3 valor fuera de rango · 4 sin enlace (llegó un ESTADO antes de haber latido válido).

### Reglas

1. La Jetson envía un **LATIDO cada ~200 ms**, idealmente desde un hilo propio, que no se bloquee mientras corre la inferencia.
2. Si el ESP32 pasa **800 ms** sin recibir un LATIDO válido, entra en fail-safe: relé apagado, LED rojo y estado SIN_ENLACE. También envía EVENTO_ENLACE(0).
3. Después de recuperar el enlace, el ESP32 **queda en ROJO** hasta recibir un nuevo ESTADO. Nunca retoma solo un VERDE anterior.
4. Un ESTADO recibido sin enlace se rechaza con NACK código 4.
5. El ESP32 sale solo del VERDE después de 8 s (`CONFIG_EPP_VERDE_MAX_MS`) y pasa a ROJO. Reenviar VERDE mientras ya está en verde **no** reinicia ese plazo.
6. Si ESTADO o BUZZER no reciben ACK en ~100 ms, la Jetson puede reintentar con un SEQ nuevo. Volver a aplicar el mismo estado no tiene efectos secundarios.
7. El ESP32 repite EVENTO_BOTON cada 500 ms, hasta 10 veces, mientras no llegue el ACK.

## 4. TELEMETRIA (42 bytes, little-endian)

| Offset | Tipo | Campo |
|---|---|---|
| 0 | u8 | versión del protocolo (1) |
| 1 | u8 | estado mostrado (0..5) |
| 2 | u8 | banderas: bit0 enlace, bit1 relé, bit2 presencia PIR, bit3 PIR listo, bit4 botón presionado |
| 3 | u8 | uso de CPU en % (255 = no disponible) |
| 4 | u32 | tiempo encendido (s) |
| 8 | u32 | heap libre (B) |
| 12 | u32 | heap mínimo histórico (B) |
| 16 | u32 | tramas válidas recibidas |
| 20 | u32 | errores de CRC |
| 24 | u16 | errores de longitud |
| 26 | u16 | tramas incompletas descartadas por timeout |
| 28 | u16 | errores del UART (desbordes, framing) |
| 30 | u16 | veces que entró en fail-safe |
| 32 | 5 × u16 | stack libre mínimo (B) de las tareas rx, enlace, salidas, entradas y telemetría |

Se envía cada 1 s y también como respuesta a PEDIR_TELEMETRIA.

## 5. Parser y resincronización

El parser es una máquina de estados que avanza byte a byte: buscar 0xAA → leer TIPO, SEQ y LEN → validar LEN ≤ 48 → leer los DATOS y el CRC → verificar el CRC.

- **Basura entre tramas:** los bytes que llegan mientras se busca 0xAA se descartan y se cuentan.
- **Falso inicio:** si la LEN es inválida o el CRC no coincide, el parser **no tira todo lo acumulado**. Descarta solo el 0xAA inicial y vuelve a examinar el resto, porque la trama verdadera puede empezar dentro de la falsa. Ejemplo: con `00 13 AA 02 07` seguido de una trama válida, el `AA` de la basura se tomaría como inicio y la trama real terminaría "comida". Las pruebas PU-11 y PI-07 cubren este caso. La implementación es iterativa, con una cola de 54 B, sin recursión ni memoria dinámica.
- **Trama incompleta:** si pasan 20 ms de silencio con una trama a medias, se descarta (`err_timeout`). A 115200 baudios, una trama máxima tarda unos 4,7 ms.
- **Efecto secundario conocido:** un falso inicio puede sumar un error extra de LEN o de CRC antes de resincronizar. Por eso los contadores son "errores detectados" y no "tramas corruptas recibidas". Las pruebas usan "aumentó en ≥ N" por esta razón.

## 6. Ejemplos de tramas (verificados por la prueba PU-18)

| Mensaje | Bytes (hex) |
|---|---|
| LATIDO seq=0 | `AA 01 00 00 AC FB` |
| ESTADO VERDE seq=1 | `AA 02 01 01 03 CA 5D` |
| ESTADO ROJO seq=2 | `AA 02 02 01 04 7D 74` |
| BUZZER ERROR seq=3 | `AA 03 03 01 04 F9 35` |
| PEDIR_TELEMETRIA seq=4 | `AA 04 04 00 98 DC` |
| ACK de ESTADO seq=1 | `AA 10 01 01 02 24 BB` |
| NACK de ESTADO, SIN_ENLACE, seq=1 | `AA 11 01 02 02 04 35 81` |
| EVENTO_PIR presencia + listo seq=7 | `AA 81 07 02 01 01 D0 85` |
| EVENTO_BOTON seq=8 | `AA 82 08 01 01 21 3E` |
| EVENTO_ENLACE perdido seq=9 | `AA 84 09 01 00 A9 3E` |
| EVENTO_ARRANQUE encendido, v1.0, seq=0 | `AA 80 00 03 01 01 00 ED 45` |

## 7. Secuencia típica

```
Jetson                                   ESP32
  |                                        |  (arranca: rojo fijo, relé OFF)
  | <------------ EVENTO_ARRANQUE -------- |
  | --- LATIDO (cada 200 ms) ------------> |
  | <------------ EVENTO_ENLACE(1) ------- |  (sigue en rojo)
  | --- ESTADO(REPOSO) ------------------> |
  | <------------ ACK -------------------- |
  | <------------ EVENTO_PIR(1,1) -------- |  (alguien llega)
  | --- ESTADO(AMARILLO_PARPADEO) -------> |
  | --- ESTADO(AMARILLO_FIJO) -----------> |  (verificando EPP)
  | --- ESTADO(VERDE) + BUZZER(OK) ------> |  (relé ON, máx. 8 s)
  | --- ESTADO(REPOSO) ------------------> |
  |        ... la Jetson se cuelga ...     |
  |                                        |  (800 ms sin latido)
  | <------------ EVENTO_ENLACE(0) ------- |  fail-safe: rojo, relé OFF
```

## 8. Cómo usar la biblioteca en el daemon C++ de la Jetson

```cmake
add_subdirectory(${CMAKE_SOURCE_DIR}/../common/protocolo protocolo)
target_link_libraries(epp_daemon PRIVATE protocolo)
```

```cpp
#include "protocolo.h"   // ya trae extern "C"
proto_trama_t t; proto_msg_estado(&t, PROTO_ST_VERDE); t.seq = seq++;
uint8_t buf[PROTO_MAX_TRAMA]; size_t n = proto_codificar(&t, buf, sizeof buf);
write(fd, buf, n);
// recepción: proto_parser_init(&p, callback, this); proto_parser_feed(&p, rx, nrx);
```

Las pruebas también se pueden correr en la Jetson (aarch64), y así se verifica la misma biblioteca en las dos arquitecturas: `cmake -S common/protocolo -B build-proto && cmake --build build-proto && ctest --test-dir build-proto`.
