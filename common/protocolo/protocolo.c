#include "protocolo.h"

uint16_t epp_crc16_ccitt_false(const uint8_t *data, size_t size) {
    if (data == NULL && size != 0u) return 0u; /* argumento invalido */
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < size; ++i) {
        crc ^= (uint16_t)((uint16_t)data[i] << 8u);
        for (unsigned bit = 0; bit < 8u; ++bit) {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1u) ^ 0x1021u)
                                  : (uint16_t)(crc << 1u);
        }
    }
    return crc;
}

size_t epp_frame_encode(uint8_t tipo, uint8_t secuencia,
                        const uint8_t *payload, uint8_t longitud,
                        uint8_t *salida, size_t capacidad) {
    if (salida == NULL || longitud > EPP_PROTO_MAX_PAYLOAD ||
        (payload == NULL && longitud != 0u)) return 0u;
    size_t total = (size_t)longitud + EPP_PROTO_OVERHEAD;
    if (capacidad < total) return 0u;
    salida[0] = EPP_PROTO_SOF0;
    salida[1] = EPP_PROTO_SOF1;
    salida[2] = EPP_PROTO_VERSION;
    salida[3] = tipo;
    salida[4] = secuencia;
    salida[5] = longitud;
    for (size_t i = 0; i < longitud; ++i) salida[6 + i] = payload[i];
    uint16_t crc = epp_crc16_ccitt_false(salida + 2, (size_t)longitud + 4u);
    salida[6 + longitud] = (uint8_t)(crc & 0xFFu);
    salida[7 + longitud] = (uint8_t)(crc >> 8u);
    return total;
}
