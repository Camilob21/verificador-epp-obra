# Borrador del protocolo UART (v1)

Formato (todos los tamaños en bytes):

| SOF0 | SOF1 | versión | tipo | secuencia | longitud | datos | CRC16 |
|---|---|---|---|---|---|---|---|
| A5 | 5A | 01 | 1 | 1 | 1 | 0–64 | 2 (LE) |

CRC16-CCITT-FALSE sobre bytes desde `versión` hasta el final de `datos`, **no** sobre SOF ni sobre los dos bytes del CRC. Valor conocido: `CRC("123456789") = 0x29B1`.

Tipos reservados: `HEARTBEAT=1`, `STATUS=2`, `COMMAND=3`, `ACK=4`. **Aún no están definidas las cargas, ACK/reintentos, timeout, parser de recepción ni el control del relé.** Hasta cerrar esos puntos, estos mensajes no pueden significar una autorización real. Parámetros físicos de UART (baudrate, pinout y timeout) **pendientes de acordar y verificar**.

El mismo `.c` se compila en el PC y en el componente ESP-IDF; no mantener dos copias independientes.
