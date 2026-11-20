#ifndef EPP_PROTOCOLO_H
#define EPP_PROTOCOLO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* BORRADOR v1: confirmar con pruebas de integración antes de utilizar actuadores.
 * Trama: SOF[2] (A5 5A), version[1], tipo[1], secuencia[1],
 * longitud[1] (0..64), carga[longitud], CRC16[2] little-endian.
 * CRC16-CCITT-FALSE: polinomio 0x1021, init 0xFFFF, sin reflexión,
 * xorout 0; calculado desde 'version' hasta último byte de carga.
 * El protocolo no está conectado al relé ni al fail-safe todavía.
 */
#define EPP_PROTO_VERSION 1u
#define EPP_PROTO_SOF0 0xA5u
#define EPP_PROTO_SOF1 0x5Au
#define EPP_PROTO_MAX_PAYLOAD 64u
#define EPP_PROTO_OVERHEAD 8u

enum epp_msg_type {
    EPP_MSG_HEARTBEAT = 1,
    EPP_MSG_STATUS = 2,
    EPP_MSG_COMMAND = 3,
    EPP_MSG_ACK = 4
};

uint16_t epp_crc16_ccitt_false(const uint8_t *data, size_t size);

/* Retorna bytes escritos o 0 en argumentos invalidos/capacidad insuficiente. */
size_t epp_frame_encode(uint8_t tipo, uint8_t secuencia,
                        const uint8_t *payload, uint8_t longitud,
                        uint8_t *salida, size_t capacidad);

#ifdef __cplusplus
}
#endif
#endif
